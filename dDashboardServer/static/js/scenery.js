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
import { parseModel, mergeMeshes, parseDds, decodeDxt, completeChain, linearColors, groupObjects, cellsOf, textureAlphaMode, gameLook, decodeSceneMap, sceneAt, loadedScenes } from '/js/scenery-core.js';

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
export function createScenery({ scene, camera, renderer, urls, focus, onProgress, onScenes }) {
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
	let requests = new AbortController(); // aborted on clear: requests for what's no longer wanted stop
	let lastUpdate = 0;
	const focusPoint = new THREE.Vector3();
	// Scenes: 'all' draws every scene's objects; 'game' the ones the game keeps loaded around the focus (the scene
	// under it, the scenes connected to it and the global scene); 'manual' the ones picked (setManualScenes)
	let sceneMode = 'all';
	let manualScenes = new Set();
	let sceneMap = null;
	let focusScene = null;      // the scene under the focus, once known
	let shownScenes = null;     // Set of scene ids drawn, null: all

	function report() {
		if (!onProgress) return;
		let done = 0, wanted = 0;
		for (const entry of assets.values()) {
			wanted++;
			if (entry.state === 'done' || entry.state === 'failed') done++;
		}
		onProgress(done, wanted);
	}

	// A model or texture URL with the manifest's conversion format, so a browser's week-long cache of models
	// converted the old way isn't used once the server converts them anew
	function versioned(url) {
		return manifest && manifest.format ? url + (url.includes('?') ? '&' : '?') + 'v=' + manifest.format : url;
	}

	async function fetchBuffer(url) {
		const response = await fetch(url, { credentials: 'same-origin', signal: requests.signal });
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
			const size = detail.texture, mine = generation;
			textures.set(key, fetchBuffer(versioned(urls.texture(manifest.zone, asset, slot, lod))).then((buffer) => {
				const texture = makeTexture(buffer, size);
				// Cleared meanwhile: the texture was already let go of (clear disposes what it finds)
				if (texture && mine === generation) used += texture.userData.bytes;
				return texture;
			}).catch(() => null));
		}
		return textures.get(key);
	}

	// Whether a mesh's vertex colors are drawn: as its game look says, else as NiVertexColorProperty does
	const usesVertexColors = (mesh, look) => (look ? look.vertexColors : !!(mesh.colors && mesh.vertexColors !== 0));

	function geometryOf(mesh, look) {
		const geometry = new THREE.BufferGeometry();
		geometry.setAttribute('position', new THREE.BufferAttribute(mesh.positions, 3));
		if (mesh.normals) geometry.setAttribute('normal', new THREE.BufferAttribute(mesh.normals, 3, true));
		if (mesh.uvs) geometry.setAttribute('uv', new THREE.BufferAttribute(mesh.uvs, 2));
		if (usesVertexColors(mesh, look)) geometry.setAttribute('color', new THREE.BufferAttribute(linearColors(mesh.colors), 4, true));
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

	// Two layer shaders: the dark texture (its own UV set) under the base one by the vertex alpha, or the two added
	const TWO_LAYERS_BLENDED_FRAGMENT = `
#ifdef USE_MAP
	#ifdef USE_COLOR_ALPHA
	float layerMix = vColor.a;
	#else
	float layerMix = 1.0;
	#endif
	diffuseColor.rgb *= mix( texture2D( darkMap, vUvDark ).rgb, texture2D( map, vMapUv ).rgb, layerMix );
#endif
`;
	const TWO_LAYERS_ADDED_FRAGMENT = `
#ifdef USE_MAP
	diffuseColor *= texture2D( map, vMapUv ) * layerWeights.x + texture2D( darkMap, vUvDark ) * layerWeights.y;
#endif
`;

	// The texture alpha mode's change to a fragment shader
	function textureAlphaPatch(shader, mode) {
		if (mode === 'decal') {
			shader.fragmentShader = shader.fragmentShader.replace('#include <map_fragment>', '').replace('#include <color_fragment>', DECAL_FRAGMENT);
		} else if (mode === 'ignored') {
			shader.fragmentShader = shader.fragmentShader.replace('#include <map_fragment>', OPAQUE_MAP_FRAGMENT);
		}
	}

	function useTextureAlpha(material, mode) {
		if (mode === 'opacity' || !material.map) return material;
		material.onBeforeCompile = (shader) => textureAlphaPatch(shader, mode);
		material.customProgramCacheKey = () => 'textureAlpha:' + mode;
		return material;
	}

	// The zone's lights as the game's shaders get them (manifest.lighting), shared by every lit material
	const gameLights = {
		gameLightColor: { value: new THREE.Vector3(1, 1, 1) }, gameAmbient: { value: new THREE.Vector3() }, gameLightVec: { value: new THREE.Vector3(0, 1, 0) },
		gameLightOn: { value: 0 } // 1 once a manifest brought the zone's lighting (the terrain uses its own light until then)
	};
	function setGameLights(lighting) {
		gameLights.gameLightOn.value = lighting ? 1 : 0;
		if (!lighting) return;
		gameLights.gameLightColor.value.fromArray(lighting.light);
		gameLights.gameAmbient.value.fromArray(lighting.ambient);
		gameLights.gameLightVec.value.fromArray(lighting.lightVec);
		lightBlend = null;
	}

	// A blend from the lights now to `lighting` over a second or two, as the client blends between scenes' lighting
	const BLEND_SECONDS = 1.5;
	let lightBlend = null;
	function blendLightsTo(lighting) {
		if (!lighting) return;
		lightBlend = {
			from: { light: gameLights.gameLightColor.value.clone(), ambient: gameLights.gameAmbient.value.clone(), lightVec: gameLights.gameLightVec.value.clone() },
			to: { light: new THREE.Vector3().fromArray(lighting.light), ambient: new THREE.Vector3().fromArray(lighting.ambient), lightVec: new THREE.Vector3().fromArray(lighting.lightVec) },
			t: 0
		};
	}
	function stepLightBlend(dt) {
		if (!lightBlend) return;
		lightBlend.t = Math.min(1, lightBlend.t + dt / BLEND_SECONDS);
		const { from, to, t } = lightBlend;
		gameLights.gameLightColor.value.lerpVectors(from.light, to.light, t);
		gameLights.gameAmbient.value.lerpVectors(from.ambient, to.ambient, t);
		gameLights.gameLightVec.value.lerpVectors(from.lightVec, to.lightVec, t).normalize();
		if (t >= 1) lightBlend = null;
	}

	/**
	 * Lighting per vertex as BasicShaders.fx and LEGOPPLighting.fx do it: sun * max(0, N.L) + ambient in the game's
	 * (sRGB) color space, which the vertex shader's color output clamps to 1, then made linear for three.js.
	 */
	const GAME_LIGHT_VERTEX = `#include <begin_vertex>
	vec3 gameNormal = normal;
	#ifdef USE_INSTANCING
	gameNormal = mat3( instanceMatrix ) * gameNormal;
	#endif
	gameNormal = normalize( mat3( modelMatrix ) * gameNormal );
	vGameLight = pow( clamp( gameLightColor * max( 0.0, dot( gameNormal, gameLightVec ) ) + gameAmbient, 0.0, 1.0 ), vec3( 2.2 ) );`;

	/**
	 * A material that draws a mesh the way its game shader does (gameLook): unlit by the view's own lights and tone
	 * mapping, the zone's sun and ambient light per vertex when the shader is lit, the material's color only when the
	 * shader reads it.
	 */
	function gameMaterial(options, mesh, alphaMode, look, darkMap = null) {
		const material = new THREE.MeshBasicMaterial({
			...options,
			color: look.material ? options.color : new THREE.Color(1, 1, 1)
		});
		material.toneMapped = false;
		const layers = darkMap ? look.layers : null;
		// TwoLayersAdded_PS: base * material diffuse red + dark * material diffuse green (their animations)
		const weights = new THREE.Vector2(mesh.diffuse[0], mesh.diffuse[1]);
		material.onBeforeCompile = (shader) => {
			if (layers) {
				shader.uniforms.darkMap = { value: darkMap };
				shader.uniforms.layerWeights = { value: weights };
				shader.vertexShader = 'attribute vec2 uvDark;\nvarying vec2 vUvDark;\n' +
					shader.vertexShader.replace('#include <uv_vertex>', '#include <uv_vertex>\n\tvUvDark = uvDark;');
				shader.fragmentShader = 'uniform sampler2D darkMap;\nuniform vec2 layerWeights;\nvarying vec2 vUvDark;\n' + shader.fragmentShader
					.replace('#include <map_fragment>', layers === 'blended' ? TWO_LAYERS_BLENDED_FRAGMENT : TWO_LAYERS_ADDED_FRAGMENT)
					.replace('#include <color_fragment>', layers === 'blended' ? '#ifdef USE_COLOR_ALPHA\n\tdiffuseColor.rgb *= vColor.rgb;\n#endif' : '#include <color_fragment>');
			} else if (options.map) {
				textureAlphaPatch(shader, alphaMode);
			}
			if (!look.lit) return;
			Object.assign(shader.uniforms, gameLights);
			shader.vertexShader = 'uniform vec3 gameLightColor;\nuniform vec3 gameAmbient;\nuniform vec3 gameLightVec;\nvarying vec3 vGameLight;\n' +
				shader.vertexShader.replace('#include <begin_vertex>', GAME_LIGHT_VERTEX);
			shader.fragmentShader = 'varying vec3 vGameLight;\n' +
				shader.fragmentShader.replace('#include <aomap_fragment>', '#include <aomap_fragment>\n\treflectedLight.indirectDiffuse *= vGameLight;');
		};
		material.customProgramCacheKey = () => 'game:' + (options.map ? alphaMode : '') + ':' + look.lit + ':' + layers;
		return material;
	}

	function materialOf(mesh, map, forSky, alphaMode = 'opacity', look = null, darkMap = null) {
		// Nearly everything in the game's files has alpha blending switched on; it only shows where something is see-
		// through: the material, a vertex or the texture (only when the object's shader uses the texture's alpha as
		// opacity). Blended meshes still write depth, as Gamebryo's default does.
		const vertexColors = usesVertexColors(mesh, look);
		let vertexAlpha = false;
		// A two layer blend reads the vertex alpha as the mix of its textures, not as opacity
		const layersBlended = !!(darkMap && look.layers === 'blended');
		if (vertexColors && !layersBlended) for (let i = 3; i < mesh.colors.length && !vertexAlpha; i += 4) vertexAlpha = mesh.colors[i] < 250;
		const textureAlpha = alphaMode === 'opacity' && !!(map && map.userData.alpha);
		// The game's shaders take alpha from the vertex colors and texture only; NiMaterialProperty's is for fixed function
		const materialAlpha = look && !look.material ? 1 : mesh.alpha;
		const seeThrough = mesh.blend && (materialAlpha < 0.99 || vertexAlpha || textureAlpha);
		const options = {
			color: new THREE.Color().setRGB(mesh.diffuse[0], mesh.diffuse[1], mesh.diffuse[2], THREE.SRGBColorSpace),
			vertexColors,
			transparent: seeThrough,
			opacity: materialAlpha,
			alphaTest: mesh.test >= 0 ? Math.max(mesh.test / 255, 0.01) : 0,
			side: mesh.doubleSided ? THREE.DoubleSide : THREE.FrontSide,
			map: map || null
		};
		if (forSky) return useTextureAlpha(new THREE.MeshBasicMaterial({ ...options, depthWrite: false, fog: false }), alphaMode);
		if (look) return gameMaterial(options, mesh, alphaMode, look, darkMap);
		const material = new THREE.MeshStandardMaterial({ ...options, roughness: 0.85, metalness: 0 });
		material.emissive.setRGB(mesh.emissive[0], mesh.emissive[1], mesh.emissive[2], THREE.SRGBColorSpace);
		return useTextureAlpha(material, alphaMode);
	}

	async function buildParts(asset, lod, forSky) {
		const buffer = await fetchBuffer(versioned(urls.mesh(manifest.zone, asset, lod)));
		const model = parseModel(buffer);
		const parts = [];
		// The sky's layers keep their order; everything else has its look-alike pieces joined
		for (const mesh of forSky ? model.meshes : mergeMeshes(model.meshes)) {
			if (!mesh.vertices || !mesh.indices.length) continue;
			// The sky keeps its own unlit look
			const look = forSky ? null : gameLook(manifest, asset, mesh);
			const textureOf = async (slot, clampU, clampV) => {
				const texture = await loadTexture(asset, slot, model.header.textures[slot], lod);
				if (!texture) return null;
				const map = texture.clone(); // shares the image; wrapping differs per mesh
				map.wrapS = clampU ? THREE.ClampToEdgeWrapping : THREE.RepeatWrapping;
				map.wrapT = clampV ? THREE.ClampToEdgeWrapping : THREE.RepeatWrapping;
				map.needsUpdate = true;
				return map;
			};
			const map = mesh.texture >= 0 && mesh.uv && (!look || look.texture) ? await textureOf(mesh.texture, mesh.clampU, mesh.clampV) : null;
			// A two layer shader's second texture, on its own UV set
			const darkMap = look && look.layers && map && mesh.darkTexture >= 0 && mesh.uvs2 ? await textureOf(mesh.darkTexture, false, false) : null;
			const geometry = geometryOf(mesh, look);
			if (darkMap) geometry.setAttribute('uvDark', new THREE.BufferAttribute(mesh.uvs2, 2));
			parts.push({ geometry, material: materialOf(mesh, map, forSky, textureAlphaMode(manifest, asset, mesh), look, darkMap) });
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
			if (mine !== generation) {
				parts.forEach((p) => { p.geometry.dispose(); if (p.material.map) p.material.map.dispose(); p.material.dispose(); });
				return;
			}
			const instances = byAsset.get(asset) || [];
			const matrix = new THREE.Matrix4(), position = new THREE.Vector3(), rotation = new THREE.Quaternion(), scale = new THREE.Vector3(), tint = new THREE.Color();
			// Objects the game doesn't draw (volumes, triggers) get their own cells in a see-through colour, shown on request
			const cells = [];
			// A cell holds one scene's objects, so scenes can be shown and hidden like the game streams them
			const byScene = new Map();
			for (const instance of instances) {
				const key = instance.scene === null || instance.scene === undefined ? -1 : instance.scene;
				if (!byScene.has(key)) byScene.set(key, []);
				byScene.get(key).push(instance);
			}
			for (const [sceneId, sceneInstances] of byScene) {
				for (const hidden of [false, true]) {
					for (const [, list] of cellsOf(sceneInstances.filter((i) => !!i.hidden === hidden), CELL)) cells.push({ hidden, list, scene: sceneId });
				}
			}
			for (const { hidden, list: cellInstances, scene: cellScene } of cells) {
				const group = new THREE.Group();
				const cellCenter = new THREE.Vector3();
				let maxScale = 0;
				for (const part of parts) {
					if (hidden && !part.ghost) part.ghost = new THREE.MeshBasicMaterial({ color: HIDDEN_COLOR, transparent: true, opacity: 0.35, depthWrite: false, side: THREE.DoubleSide });
					const mesh = new THREE.InstancedMesh(part.geometry, hidden ? part.ghost : part.material, cellInstances.length);
					mesh.userData.asset = asset; // which of the manifest's models it is, for picking
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
				entry.cells.push({ group, center: cellCenter, radius: reach + radius * maxScale, hidden, scene: cellScene });
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
			for (const i of instances) if ((showHidden || !i.hidden) && sceneShown(i.scene)) best = Math.min(best, Math.hypot(i.x - at.x, i.z - at.z));
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

	// Objects of no known scene (older manifests) are always drawn
	function sceneShown(id) {
		return !shownScenes || id === null || id === undefined || id < 0 || shownScenes.has(id);
	}

	// The scenes to draw and the lighting, from the mode and the scene under the focus; tells onScenes when they change
	function updateScenes(force = false) {
		if (!manifest) return;
		const at = focus ? focus(focusPoint) || camera.position : camera.position;
		const under = sceneMap ? sceneAt(sceneMap, at.x, at.z) : null;
		let shown = null;
		if (sceneMode === 'game' && manifest.scenes) shown = loadedScenes(manifest.scenes, under === null ? 0 : under);
		else if (sceneMode === 'manual') shown = new Set(manualScenes);
		const same = (a, b) => (a === b) || (a && b && a.size === b.size && [...a].every((v) => b.has(v)));
		if (!force && under === focusScene && same(shown, shownScenes)) return;
		focusScene = under;
		shownScenes = shown;
		// The game blends to the lighting of the scene the player walks into
		const sceneEntry = manifest.scenes && under !== null ? manifest.scenes.find((s) => s.id === under) : null;
		blendLightsTo(sceneEntry && sceneEntry.lighting ? sceneEntry.lighting : manifest.lighting);
		want();
		updateVisibility();
		if (onScenes) onScenes({ mode: sceneMode, scene: under, shown: shownScenes, scenes: manifest.scenes || [] });
	}

	function updateVisibility() {
		const eye = camera.position;
		for (const entry of assets.values()) {
			for (const cell of entry.cells) cell.group.visible = enabled && (!cell.hidden || showHidden) && sceneShown(cell.scene) && eye.distanceTo(cell.center) - cell.radius < drawDistance();
		}
	}

	function clear() {
		generation++;
		requests.abort();
		requests = new AbortController();
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
			const mine = generation;
			detail = DETAIL[Math.max(0, Math.min(DETAIL.length - 1, level))];
			if (!manifest || manifest.url !== url) {
				manifest = null;
				let loaded = null;
				try {
					const response = await fetch(url, { credentials: 'same-origin', signal: requests.signal });
					if (response.ok) loaded = await response.json();
				} catch (error) {
					// Aborted by a newer load or clear, or the network failed
				}
				// Another load or a clear came first: this one's result isn't wanted any more
				if (mine !== generation) return false;
				if (!loaded) return false;
				manifest = loaded;
				manifest.url = url;
				setGameLights(manifest.lighting);
				sceneMap = decodeSceneMap(manifest.sceneMap);
				focusScene = null;
				byAsset = groupObjects(manifest.objects);
				if (manifest.sky >= 0) byAsset.delete(manifest.sky);
			}
			loadSky();
			updateScenes(true);
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
			stepLightBlend(dt);
			lastUpdate += dt;
			if (lastUpdate < UPDATE_SECONDS) return;
			lastUpdate = 0;
			updateScenes();
			want();
			updateVisibility();
		},
		/**
		 * Which scenes' objects are drawn: 'all', 'game' (as the game streams them around the focus: the scene under
		 * it, the ones connected to it and the global scene) or 'manual' (setManualScenes).
		 */
		setSceneMode(mode) {
			sceneMode = mode === 'game' || mode === 'manual' ? mode : 'all';
			updateScenes(true);
		},
		setManualScenes(ids) {
			manualScenes = new Set(ids);
			if (sceneMode === 'manual') updateScenes(true);
		},
		/** {mode, scene (under the focus, null without a scene map), shown (Set, null: all), scenes (manifest.scenes)} */
		sceneState() {
			return { mode: sceneMode, scene: focusScene, shown: shownScenes, scenes: manifest && manifest.scenes ? manifest.scenes : [] };
		},
		/** The zone's lights as uniforms (updated in place, blends too), for the terrain to be lit like the scenery. */
		gameLights() { return gameLights; },
		/** How many objects the loaded manifest places. */
		count() { return manifest ? manifest.objects.asset.length : 0; },
		stats() {
			let cells = 0, drawn = 0;
			for (const entry of assets.values()) for (const cell of entry.cells) { cells++; if (cell.group.visible) drawn++; }
			return { assets: assets.size, loaded: [...assets.values()].filter((e) => e.state === 'done').length, cells, drawn, megabytes: Math.round(used / 1048576) };
		},
		/** Let go of everything loaded and stop what's loading (switching zones); load() starts again. */
		clear() {
			clear();
			manifest = null;
			byAsset = new Map();
			sceneMap = null;
			focusScene = shownScenes = null;
		},
		dispose() {
			clear();
			scene.remove(root);
			scene.remove(sky);
		}
	};
}
