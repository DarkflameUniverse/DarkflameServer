/**
 * A zone's scenery for the 3D views (ES module, three.js): every scene object's model and the sky, the way the game
 * client draws them, from the server's scenery manifest (/api/properties/:id/scenery, /api/world3d/:zone/scenery).
 * The flairs' manifest (/api/world3d/:zone/flairs) has the same form plus a tint per object and a shorter draw distance.
 *
 * Models load nearest first around a focus point (the camera's target), a few at a time, up to the detail level's
 * draw distance and memory budget. Each model's objects are drawn as InstancedMeshes, one per mesh part and square
 * cell of the map, so three.js can skip cells outside the view and cells past the draw distance are hidden. The sky
 * follows the camera, far away, behind everything.
 */
import * as THREE from 'three';
import { parseModel, mergeMeshes, parseDds, decodeDxt, completeChain, linearColors, groupObjects, cellsOf, textureAlphaMode } from '/js/scenery-core.js';

// Per detail level (the property view's 0 high, 1 medium, 2 low): the model LOD, how far objects are drawn, the
// largest texture side and a memory budget for geometry and textures
export const DETAIL = [
	{ lod: 0, distance: 1400, texture: 512, budget: 384 << 20 },
	{ lod: 1, distance: 800, texture: 256, budget: 192 << 20 },
	{ lod: 2, distance: 450, texture: 128, budget: 96 << 20 }
];
const CELL = 128;
const HIDDEN_COLOR = 0xff4fd8;
const PARALLEL = 4;
const UPDATE_SECONDS = 0.5;

/**
 * scene, camera, renderer: the view's; urls: {manifest, mesh(zone, asset, lod), texture(zone, asset, slot, lod)};
 * focus(): where to load around (defaults to the camera); onProgress(loaded, wanted).
 */
export function createScenery({ scene, camera, renderer, urls, focus, onProgress }) {
	const root = new THREE.Group();
	root.name = 'scenery';
	scene.add(root);
	const sky = new THREE.Group();
	sky.name = 'sky';
	scene.add(sky);

	const extensions = {
		s3tc: renderer.extensions.has('WEBGL_compressed_texture_s3tc'),
		s3tcSrgb: renderer.extensions.has('WEBGL_compressed_texture_s3tc_srgb')
	};
	let manifest = null;
	let byAsset = new Map();     // asset -> instances
	let nearest = new Map();     // asset -> distance of its nearest instance to the last focus
	let detail = DETAIL[2];
	let enabled = true;
	let showHidden = false;
	let skyOn = true;      // also draw the objects the game doesn't (manifest objects.hidden)
	const assets = new Map();    // asset -> {state: 'queued'|'loading'|'done'|'failed', cells: [{group, center, radius}], bytes}
	const textures = new Map();  // "lod/path" -> Promise<Texture|null>
	let used = 0;                // bytes of geometry and textures on the GPU
	let active = 0;
	let generation = 0;          // bumped on clear, so late responses are dropped
	let lastUpdate = 0;
	const focusPoint = new THREE.Vector3();

	function report() {
		if (!onProgress) return;
		let done = 0, wanted = 0;
		for (const entry of assets.values()) {
			wanted++;
			if (entry.state === 'done' || entry.state === 'failed') done++;
		}
		onProgress(done, wanted);
	}

	async function fetchBuffer(url) {
		const response = await fetch(url, { credentials: 'same-origin' });
		if (!response.ok) throw new Error(response.status + ' ' + url);
		return response.arrayBuffer();
	}

	// A DDS as a texture: compressed on GPUs that take S3TC (sRGB too), else decoded to RGBA here
	function makeTexture(buffer, maxSize) {
		const dds = parseDds(buffer, maxSize);
		if (!dds) return null;
		let texture;
		if (dds.format !== 'RGBA' && extensions.s3tc && extensions.s3tcSrgb && dds.width % 4 === 0 && dds.height % 4 === 0) {
			const format = dds.format === 'DXT1' ? THREE.RGBA_S3TC_DXT1_Format : dds.format === 'DXT3' ? THREE.RGBA_S3TC_DXT3_Format : THREE.RGBA_S3TC_DXT5_Format;
			const levels = completeChain(dds.levels) ? dds.levels : [dds.levels[0]];
			texture = new THREE.CompressedTexture(levels.map((l) => ({ data: l.data, width: l.width, height: l.height })), dds.width, dds.height, format);
			texture.minFilter = levels.length > 1 ? THREE.LinearMipmapLinearFilter : THREE.LinearFilter;
			texture.userData.bytes = levels.reduce((sum, l) => sum + l.data.byteLength, 0);
		} else {
			const level = dds.levels[0];
			const rgba = dds.format === 'RGBA' ? level.data : decodeDxt(dds.format, level.width, level.height, level.data);
			texture = new THREE.DataTexture(rgba, level.width, level.height, THREE.RGBAFormat);
			texture.generateMipmaps = true;
			texture.minFilter = THREE.LinearMipmapLinearFilter;
			texture.userData.bytes = rgba.byteLength * 4 / 3;
		}
		// DXT1's one-bit alpha is for cut-outs (alpha tested); DXT3/5 and 32-bit files carry real transparency
		texture.userData.alpha = dds.format === 'DXT3' || dds.format === 'DXT5' || (dds.format === 'RGBA' && dds.alpha);
		texture.magFilter = THREE.LinearFilter;
		texture.colorSpace = THREE.SRGBColorSpace;
		// DDS rows run top down and the game's UVs start at the top, so no flip
		texture.flipY = false;
		texture.anisotropy = 4;
		texture.needsUpdate = true;
		return texture;
	}

	function loadTexture(asset, slot, path, lod) {
		// Textures stored inside a .nif are named by their block ("#12"), so only unique within their model
		const key = lod + '/' + (path.startsWith('#') ? asset + path : path);
		if (!textures.has(key)) {
			const size = detail.texture;
			textures.set(key, fetchBuffer(urls.texture(manifest.zone, asset, slot, lod)).then((buffer) => {
				const texture = makeTexture(buffer, size);
				if (texture) used += texture.userData.bytes;
				return texture;
			}).catch(() => null));
		}
		return textures.get(key);
	}

	function geometryOf(mesh) {
		const geometry = new THREE.BufferGeometry();
		geometry.setAttribute('position', new THREE.BufferAttribute(mesh.positions, 3));
		if (mesh.normals) geometry.setAttribute('normal', new THREE.BufferAttribute(mesh.normals, 3, true));
		if (mesh.uvs) geometry.setAttribute('uv', new THREE.BufferAttribute(mesh.uvs, 2));
		if (mesh.colors && mesh.vertexColors !== 0) geometry.setAttribute('color', new THREE.BufferAttribute(linearColors(mesh.colors), 4, true));
		geometry.setIndex(new THREE.BufferAttribute(mesh.indices, 1));
		if (!mesh.normals) geometry.computeVertexNormals();
		geometry.computeBoundingSphere();
		return geometry;
	}

	// The game's LEGO shaders lay the texture over the vertex colors by its alpha instead of letting it show through
	const DECAL_FRAGMENT = `
#if defined( USE_COLOR_ALPHA )
	diffuseColor *= vColor;
#elif defined( USE_COLOR )
	diffuseColor.rgb *= vColor;
#endif
#ifdef USE_MAP
	vec4 sampledDiffuseColor = texture2D( map, vMapUv );
	#if defined( USE_COLOR ) || defined( USE_COLOR_ALPHA )
	diffuseColor.rgb = mix( diffuseColor.rgb, sampledDiffuseColor.rgb, sampledDiffuseColor.a );
	#else
	diffuseColor.rgb *= sampledDiffuseColor.rgb;
	#endif
#endif
`;
	// ... or leave its alpha out altogether (LEGO items, terrain meshes)
	const OPAQUE_MAP_FRAGMENT = `
#ifdef USE_MAP
	diffuseColor.rgb *= texture2D( map, vMapUv ).rgb;
#endif
`;

	function useTextureAlpha(material, mode) {
		if (mode === 'opacity' || !material.map) return material;
		material.onBeforeCompile = (shader) => {
			if (mode === 'decal') {
				shader.fragmentShader = shader.fragmentShader.replace('#include <map_fragment>', '').replace('#include <color_fragment>', DECAL_FRAGMENT);
			} else {
				shader.fragmentShader = shader.fragmentShader.replace('#include <map_fragment>', OPAQUE_MAP_FRAGMENT);
			}
		};
		material.customProgramCacheKey = () => 'textureAlpha:' + mode;
		return material;
	}

	function materialOf(mesh, map, forSky, alphaMode = 'opacity') {
		// Nearly everything in the game's files has alpha blending switched on; it only shows where something is see-
		// through: the material, a vertex or the texture (only when the object's shader uses the texture's alpha as
		// opacity). Blended meshes still write depth, as Gamebryo's default does.
		let vertexAlpha = false;
		if (mesh.colors && mesh.vertexColors !== 0) for (let i = 3; i < mesh.colors.length && !vertexAlpha; i += 4) vertexAlpha = mesh.colors[i] < 250;
		const textureAlpha = alphaMode === 'opacity' && !!(map && map.userData.alpha);
		const seeThrough = mesh.blend && (mesh.alpha < 0.99 || vertexAlpha || textureAlpha);
		const options = {
			color: new THREE.Color().setRGB(mesh.diffuse[0], mesh.diffuse[1], mesh.diffuse[2], THREE.SRGBColorSpace),
			vertexColors: !!(mesh.colors && mesh.vertexColors !== 0),
			transparent: seeThrough,
			opacity: mesh.alpha,
			alphaTest: mesh.test >= 0 ? Math.max(mesh.test / 255, 0.01) : 0,
			side: mesh.doubleSided ? THREE.DoubleSide : THREE.FrontSide,
			map: map || null
		};
		if (forSky) return useTextureAlpha(new THREE.MeshBasicMaterial({ ...options, depthWrite: false, fog: false }), alphaMode);
		const material = new THREE.MeshStandardMaterial({ ...options, roughness: 0.85, metalness: 0 });
		material.emissive.setRGB(mesh.emissive[0], mesh.emissive[1], mesh.emissive[2], THREE.SRGBColorSpace);
		return useTextureAlpha(material, alphaMode);
	}

	async function buildParts(asset, lod, forSky) {
		const buffer = await fetchBuffer(urls.mesh(manifest.zone, asset, lod));
		const model = parseModel(buffer);
		const parts = [];
		// The sky's layers keep their order; everything else has its look-alike pieces joined
		for (const mesh of forSky ? model.meshes : mergeMeshes(model.meshes)) {
			if (!mesh.vertices || !mesh.indices.length) continue;
			let map = null;
			if (mesh.texture >= 0 && mesh.uv) {
				const texture = await loadTexture(asset, mesh.texture, model.header.textures[mesh.texture], lod);
				if (texture) {
					map = texture.clone(); // shares the image; wrapping differs per mesh
					map.wrapS = mesh.clampU ? THREE.ClampToEdgeWrapping : THREE.RepeatWrapping;
					map.wrapT = mesh.clampV ? THREE.ClampToEdgeWrapping : THREE.RepeatWrapping;
					map.needsUpdate = true;
				}
			}
			const geometry = geometryOf(mesh);
			parts.push({ geometry, material: materialOf(mesh, map, forSky, textureAlphaMode(manifest, asset, mesh)) });
		}
		const min = model.header.min, max = model.header.max;
		const radius = Math.hypot(max[0] - min[0], max[1] - min[1], max[2] - min[2]) / 2;
		const center = new THREE.Vector3((min[0] + max[0]) / 2, (min[1] + max[1]) / 2, (min[2] + max[2]) / 2);
		return { parts, radius, center };
	}

	async function loadAsset(asset) {
		const entry = assets.get(asset);
		const mine = generation;
		entry.state = 'loading';
		active++;
		try {
			const { parts, radius, center } = await buildParts(asset, detail.lod, false);
			if (mine !== generation) { parts.forEach((p) => { p.geometry.dispose(); p.material.dispose(); }); return; }
			const instances = byAsset.get(asset) || [];
			const matrix = new THREE.Matrix4(), position = new THREE.Vector3(), rotation = new THREE.Quaternion(), scale = new THREE.Vector3(), tint = new THREE.Color();
			// Objects the game doesn't draw (volumes, triggers) get their own cells in a see-through colour, shown on request
			const cells = [];
			for (const hidden of [false, true]) {
				for (const [, list] of cellsOf(instances.filter((i) => !!i.hidden === hidden), CELL)) cells.push({ hidden, list });
			}
			for (const { hidden, list: cellInstances } of cells) {
				const group = new THREE.Group();
				const cellCenter = new THREE.Vector3();
				let maxScale = 0;
				for (const part of parts) {
					if (hidden && !part.ghost) part.ghost = new THREE.MeshBasicMaterial({ color: HIDDEN_COLOR, transparent: true, opacity: 0.35, depthWrite: false, side: THREE.DoubleSide });
					const mesh = new THREE.InstancedMesh(part.geometry, hidden ? part.ghost : part.material, cellInstances.length);
					cellInstances.forEach((instance, i) => {
						position.set(instance.x, instance.y, instance.z);
						rotation.set(instance.qx, instance.qy, instance.qz, instance.qw);
						scale.setScalar(instance.scale || 1);
						mesh.setMatrixAt(i, matrix.compose(position, rotation, scale));
						if (instance.color) {
							const k = manifest.colorScale || 1 / 255;
							mesh.setColorAt(i, tint.setRGB(instance.color[0] * k, instance.color[1] * k, instance.color[2] * k));
						}
					});
					mesh.instanceMatrix.needsUpdate = true;
					mesh.computeBoundingSphere();
					// Scenery takes the models' shadows but casts none: a shadow pass over the zone costs more than it shows
					mesh.castShadow = false;
					mesh.receiveShadow = true;
					group.add(mesh);
				}
				for (const instance of cellInstances) {
					cellCenter.add(position.set(instance.x, instance.y, instance.z));
					maxScale = Math.max(maxScale, instance.scale || 1);
				}
				cellCenter.divideScalar(cellInstances.length).add(center);
				root.add(group);
				// How far past the cell's centre its objects reach
				let reach = 0;
				for (const instance of cellInstances) reach = Math.max(reach, position.set(instance.x, instance.y, instance.z).distanceTo(cellCenter));
				entry.cells.push({ group, center: cellCenter, radius: reach + radius * maxScale, hidden });
			}
			entry.parts = parts;
			entry.bytes = parts.reduce((sum, p) => sum + p.geometry.attributes.position.array.byteLength * 2 + p.geometry.index.array.byteLength, 0);
			used += entry.bytes;
			entry.state = 'done';
			updateVisibility();
		} catch (error) {
			entry.state = 'failed';
		} finally {
			active--;
			report();
			pump();
		}
	}

	async function loadSky() {
		if (!manifest || manifest.sky < 0) return;
		const mine = generation;
		try {
			const { parts } = await buildParts(manifest.sky, 0, true);
			if (mine !== generation) return;
			parts.forEach((part, i) => {
				const mesh = new THREE.Mesh(part.geometry, part.material);
				// Its layers are drawn in the file's order, as the game does (they're all at the same distance)
				mesh.renderOrder = -1000 + i;
				mesh.frustumCulled = false;
				sky.add(mesh);
			});
		} catch (error) {
			// No sky: the plain background stays
		}
	}

	function pump() {
		if (!enabled || !manifest) return;
		const queued = [...assets.entries()].filter(([, e]) => e.state === 'queued').sort((a, b) => nearest.get(a[0]) - nearest.get(b[0]));
		for (const [asset] of queued) {
			if (active >= PARALLEL) break;
			if (used > detail.budget) break;
			loadAsset(asset);
		}
	}

	// Queue every model with an object within the draw distance of the focus
	function want() {
		if (!manifest) return;
		const at = focus ? focus(focusPoint) || camera.position : camera.position;
		for (const [asset, instances] of byAsset) {
			let best = Infinity;
			for (const i of instances) if (showHidden || !i.hidden) best = Math.min(best, Math.hypot(i.x - at.x, i.z - at.z));
			nearest.set(asset, best);
			if (best <= drawDistance() && !assets.has(asset)) assets.set(asset, { state: 'queued', cells: [], bytes: 0 });
		}
		report();
		pump();
	}

	// The detail level's draw distance, or the manifest's own when shorter (flairs)
	function drawDistance() {
		return manifest && manifest.distance ? Math.min(detail.distance, manifest.distance) : detail.distance;
	}

	function updateVisibility() {
		const eye = camera.position;
		for (const entry of assets.values()) {
			for (const cell of entry.cells) cell.group.visible = enabled && (!cell.hidden || showHidden) && eye.distanceTo(cell.center) - cell.radius < drawDistance();
		}
	}

	function clear() {
		generation++;
		for (const entry of assets.values()) {
			for (const cell of entry.cells) {
				root.remove(cell.group);
				cell.group.children.forEach((mesh) => mesh.dispose());
			}
			for (const part of entry.parts || []) {
				if (part.ghost) part.ghost.dispose();
				part.geometry.dispose();
				if (part.material.map) part.material.map.dispose();
				part.material.dispose();
			}
		}
		assets.clear();
		for (const promise of textures.values()) promise.then((t) => t && t.dispose());
		textures.clear();
		sky.children.slice().forEach((mesh) => { sky.remove(mesh); mesh.geometry.dispose(); mesh.material.dispose(); });
		used = 0;
		report();
	}

	return {
		/** Load a manifest (or reload it at another detail level: 0 high, 1 medium, 2 low). */
		async load(url = urls.manifest, level = 2) {
			clear();
			detail = DETAIL[Math.max(0, Math.min(DETAIL.length - 1, level))];
			if (!manifest || manifest.url !== url) {
				const response = await fetch(url, { credentials: 'same-origin' });
				if (!response.ok) { manifest = null; return false; }
				manifest = await response.json();
				manifest.url = url;
				byAsset = groupObjects(manifest.objects);
				if (manifest.sky >= 0) byAsset.delete(manifest.sky);
			}
			loadSky();
			want();
			return true;
		},
		setDetail(level) {
			const next = DETAIL[Math.max(0, Math.min(DETAIL.length - 1, level))];
			if (next === detail || !manifest) return;
			this.load(manifest.url, level);
		},
		/** Also draw the objects the game doesn't draw (trigger and blocking volumes), see-through. */
		setShowHidden(on) {
			showHidden = !!on;
			want();
			updateVisibility();
		},
		/** Show or hide the sky (it also goes with the whole scenery). */
		setSky(on) {
			skyOn = !!on;
			sky.visible = enabled && skyOn;
		},
		setEnabled(on) {
			enabled = on;
			root.visible = on;
			sky.visible = on && skyOn;
			if (on) want();
		},
		/** Call every frame: the sky follows the camera; every half second, nearby models load and far cells hide. */
		update(dt = 0) {
			if (!enabled) return;
			// The sky sits just inside the far plane, around the camera
			const far = Math.max(camera.far, drawDistance() * 1.6);
			if (camera.far < far) { camera.far = far; camera.updateProjectionMatrix(); }
			sky.position.copy(camera.position);
			sky.scale.setScalar(camera.far * 0.8);
			lastUpdate += dt;
			if (lastUpdate < UPDATE_SECONDS) return;
			lastUpdate = 0;
			want();
			updateVisibility();
		},
		/** How many objects the loaded manifest places. */
		count() { return manifest ? manifest.objects.asset.length : 0; },
		stats() {
			let cells = 0, drawn = 0;
			for (const entry of assets.values()) for (const cell of entry.cells) { cells++; if (cell.group.visible) drawn++; }
			return { assets: assets.size, loaded: [...assets.values()].filter((e) => e.state === 'done').length, cells, drawn, megabytes: Math.round(used / 1048576) };
		},
		dispose() {
			clear();
			scene.remove(root);
			scene.remove(sky);
		}
	};
}
