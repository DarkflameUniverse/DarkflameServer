/**
 * 3D viewer for LEGO Universe property models (ES module, three.js).
 *
 * Models are LXFML: v5 (player-built and most prebuilt models) lists parts with a design ID, materials and a
 * bone transform; v4 (older prebuilt models) nests parts in groups with axis-angle transforms. Brick geometry
 * comes from the client's LDD primitives (<designID>.g, .g1, ...), bundled per design by /api/bricks/:lod/:design.
 *
 * Each model is placed with the position and rotation stored for it in properties_contents: player-built
 * models are normalized so their LXFML is centred on the model origin, and the stored transform places it.
 *
 * Identical bricks (same design, sub-part and material) across all models are drawn as one InstancedMesh, so
 * properties with thousands of bricks stay fast. Click a model to select it; double-click to focus it.
 */
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { parseModel, mergeMeshes, linearColors, nearPlaneFor, addGlitter, glitterSettings } from '/js/scenery-core.js';

const GEOMETRY_MAGIC = 0x42473031; // "10GB"
const MAX_PARALLEL_FETCHES = 6;

// ---- Loading ----

let active = 0;
const queue = [];
function limitedFetch(url) {
	return new Promise((resolve, reject) => {
		queue.push({ url, resolve, reject });
		pump();
	});
}
function pump() {
	while (active < MAX_PARALLEL_FETCHES && queue.length) {
		const job = queue.shift();
		active++;
		fetch(job.url, { credentials: 'same-origin' }).then(job.resolve, job.reject).finally(() => { active--; pump(); });
	}
}

/** Parse one LDD .g geometry file. */
export function parseGeometry(buffer) {
	const view = new DataView(buffer);
	if (buffer.byteLength < 16 || view.getInt32(0, true) !== GEOMETRY_MAGIC) return null;
	const vertexCount = view.getInt32(4, true);
	const indexCount = view.getInt32(8, true);
	const options = view.getInt32(12, true);
	let offset = 16;

	const positions = new Float32Array(buffer.slice(offset, offset + vertexCount * 12));
	offset += vertexCount * 12;
	const normals = new Float32Array(buffer.slice(offset, offset + vertexCount * 12));
	offset += vertexCount * 12;
	// Texture coordinates are present for decorated parts
	if ((options & 3) === 3) offset += vertexCount * 8;
	const indices = new Uint32Array(buffer.slice(offset, offset + indexCount * 4));

	const geometry = new THREE.BufferGeometry();
	geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
	geometry.setAttribute('normal', new THREE.BufferAttribute(normals, 3));
	geometry.setIndex(new THREE.BufferAttribute(indices, 1));
	geometry.computeBoundingBox();
	return geometry;
}

const geometryCache = new Map(); // "lod/design" -> Promise<BufferGeometry[]>

/** Split the server's bundle of a design's .g files: uint32 count, then per part uint32 length + bytes. */
export function parseBundle(buffer) {
	const view = new DataView(buffer);
	const count = view.getUint32(0, true);
	const parts = [];
	let offset = 4;
	for (let i = 0; i < count && offset + 4 <= buffer.byteLength; i++) {
		const length = view.getUint32(offset, true);
		offset += 4;
		const geometry = parseGeometry(buffer.slice(offset, offset + length));
		if (geometry) parts.push(geometry);
		offset += length;
	}
	return parts;
}

/** All geometry parts of a design (<id>.g, <id>.g1, ...) in one request, from brickUrl(lod, design) when given. */
function loadDesign(designId, lod, brickUrl) {
	const key = `${lod}/${designId}`;
	if (!geometryCache.has(key)) {
		geometryCache.set(key, limitedFetch(brickUrl ? brickUrl(lod, designId) : `/api/bricks/${lod}/${designId}`)
			.then((response) => (response.ok ? response.arrayBuffer().then(parseBundle) : [])));
	}
	return geometryCache.get(key);
}

// ---- LXFML parsing ----

/** LXFML v5 transform: row-major 3x3 rotation followed by a translation. */
function boneMatrix(transformation) {
	const t = transformation.split(',').map(Number);
	return new THREE.Matrix4().set(
		t[0], t[3], t[6], t[9],
		t[1], t[4], t[7], t[10],
		t[2], t[5], t[8], t[11],
		0, 0, 0, 1);
}

/** LXFML v4 transform: rotate `angle` degrees around (ax, ay, az), then translate. */
function axisAngleMatrix(el) {
	const n = (name) => parseFloat(el.getAttribute(name)) || 0;
	const axis = new THREE.Vector3(n('ax'), n('ay'), n('az'));
	const rotation = new THREE.Matrix4();
	if (axis.lengthSq() > 0) rotation.makeRotationAxis(axis.normalize(), THREE.MathUtils.degToRad(n('angle')));
	return new THREE.Matrix4().makeTranslation(n('tx'), n('ty'), n('tz')).multiply(rotation);
}

/** Parts of a model: [{designId, materials, matrix}] in the model's own coordinates. */
export function parseLxfml(text) {
	const doc = new DOMParser().parseFromString(text, 'application/xml');
	const parts = [];
	const add = (designId, materials, matrix) => {
		if (!designId || !/^\d+$/.test(designId)) return;
		// Material 0 means "same as the base material"
		parts.push({ designId, materials: materials.map((m) => (m === '0' ? materials[0] : m)), matrix });
	};

	const v5Parts = doc.querySelectorAll('Bricks > Brick > Part');
	if (v5Parts.length) {
		v5Parts.forEach((part) => {
			const bone = part.querySelector('Bone');
			if (!bone) return;
			const materials = (part.getAttribute('materials') || part.getAttribute('materialID') || '0').split(',');
			add(part.getAttribute('designID'), materials, boneMatrix(bone.getAttribute('transformation')));
		});
		return parts;
	}

	const walk = (el, parent) => {
		for (const child of el.children) {
			if (child.tagName !== 'Group' && child.tagName !== 'Part') continue;
			const matrix = parent.clone().multiply(axisAngleMatrix(child));
			if (child.tagName === 'Group') walk(child, matrix);
			else add(child.getAttribute('designID'), [child.getAttribute('materialID') || '0'], matrix);
		}
	};
	doc.querySelectorAll('Scene > Model').forEach((model) => walk(model, new THREE.Matrix4()));
	return parts;
}

/**
 * The parts of a model the UGC server made (a .nif, converted by the dashboard like the scenery's models: see
 * scenery-core.js), as [{geometry, material}] in the model's own coordinates. Each part owns its geometry and material.
 */
export async function loadGeneratedModel(url) {
	const response = await limitedFetch(url);
	if (!response.ok) throw new Error('HTTP ' + response.status);
	const model = parseModel(await response.arrayBuffer());
	const parts = [];
	for (const mesh of mergeMeshes(model.meshes)) {
		if (!mesh.vertices || !mesh.indices.length) continue;
		const geometry = new THREE.BufferGeometry();
		geometry.setAttribute('position', new THREE.BufferAttribute(mesh.positions, 3));
		if (mesh.normals) geometry.setAttribute('normal', new THREE.BufferAttribute(mesh.normals, 3, true));
		const hasColors = !!(mesh.colors && mesh.vertexColors !== 0);
		if (hasColors) geometry.setAttribute('color', new THREE.BufferAttribute(linearColors(mesh.colors), 4, true));
		geometry.setIndex(new THREE.BufferAttribute(mesh.indices, 1));
		if (!mesh.normals) geometry.computeVertexNormals();
		geometry.computeBoundingBox();
		const color = new THREE.Color().setRGB(mesh.diffuse[0], mesh.diffuse[1], mesh.diffuse[2], THREE.SRGBColorSpace);
		const material = new THREE.MeshStandardMaterial({ color, vertexColors: hasColors, transparent: !!mesh.blend, roughness: 0.5, metalness: 0 });
		parts.push({ geometry, material });
	}
	if (!parts.length) throw new Error('empty model');
	return parts;
}

// ---- Viewer ----

const materialCache = new Map();
// The glitter colours' flecks and sparkles (window.LDD_GLITTER, the UGC server's current glitter settings), the
// sparkles moved each frame
const glitterMaterials = [];
function material(id) {
	if (!materialCache.has(id)) {
		const c = (window.LDD_MATERIALS || {})[id] || [160, 160, 160, 255];
		const transparent = c[3] < 255;
		const created = new THREE.MeshPhysicalMaterial({
			color: new THREE.Color().setRGB(c[0] / 255, c[1] / 255, c[2] / 255, THREE.SRGBColorSpace),
			roughness: 0.28,
			metalness: 0,
			clearcoat: 0.3,
			clearcoatRoughness: 0.25,
			transparent,
			opacity: c[3] / 255,
			depthWrite: !transparent
		});
		const glitter = window.LDD_GLITTER;
		if (glitter && (glitter.colors || []).includes(Number(id))) {
			// The flecks still and the sparkles flashing, as the game draws them on a placed model; the sparkles white
			// taking the tint of the brick's color (UgcGlitter::SparkleColor)
			const settings = glitterSettings();
			const tint = Math.min(Math.max(settings.sparkleTint / 100, 0), 1), bright = Math.min(Math.max(settings.sparkleBrightness / 100, 0), 1);
			const sparkleColor = [0, 1, 2].map((i) => Math.min(1, ((1 - tint) + tint * c[i] / 255) * bright));
			const linear = new THREE.Color().setRGB(sparkleColor[0], sparkleColor[1], sparkleColor[2], THREE.SRGBColorSpace);
			glitterMaterials.push(addGlitter(created, { coordinates: 'position', ...settings, sparkleColor: [linear.r, linear.g, linear.b] }));
		}
		materialCache.set(id, created);
	}
	return materialCache.get(id);
}

/**
 * Terrain for the viewer from /api/properties/:id/terrain: a height grid (16-bit, base64) over the whole zone.
 * Only the area around `box` (plus a margin) becomes a mesh.
 */
function buildTerrain(t, box, margin) {
	const bytes = atob(t.heights);
	const raw = new Uint16Array(t.width * t.height);
	for (let i = 0; i < raw.length; i++) raw[i] = bytes.charCodeAt(i * 2) | (bytes.charCodeAt(i * 2 + 1) << 8);
	const clampIndex = (value, max) => Math.max(0, Math.min(max - 1, value));
	const x0 = clampIndex(Math.floor((box.min.x - margin - t.minX) / t.step), t.width), x1 = clampIndex(Math.ceil((box.max.x + margin - t.minX) / t.step), t.width);
	const z0 = clampIndex(Math.floor((box.min.z - margin - t.minZ) / t.step), t.height), z1 = clampIndex(Math.ceil((box.max.z + margin - t.minZ) / t.step), t.height);
	const cols = x1 - x0 + 1, rows = z1 - z0 + 1;
	if (cols < 2 || rows < 2) return null;

	const positions = new Float32Array(cols * rows * 3);
	const colors = new Float32Array(cols * rows * 3);
	const valid = new Uint8Array(cols * rows);
	const low = new THREE.Color(0x3f6b35), mid = new THREE.Color(0x6f8f4a), high = new THREE.Color(0x9a8f78);
	let minY = Infinity, maxY = -Infinity;
	for (let r = 0; r < rows; r++) {
		for (let c = 0; c < cols; c++) {
			const value = raw[(z0 + r) * t.width + (x0 + c)];
			const i = r * cols + c;
			if (value === 65535) continue;
			const y = t.minY + (value / 65534) * (t.maxY - t.minY);
			valid[i] = 1;
			positions.set([t.minX + (x0 + c) * t.step, y, t.minZ + (z0 + r) * t.step], i * 3);
			minY = Math.min(minY, y); maxY = Math.max(maxY, y);
		}
	}
	const range = Math.max(maxY - minY, 1);
	for (let i = 0; i < cols * rows; i++) {
		if (!valid[i]) continue;
		const f = (positions[i * 3 + 1] - minY) / range;
		const color = f < 0.5 ? low.clone().lerp(mid, f * 2) : mid.clone().lerp(high, (f - 0.5) * 2);
		colors.set([color.r, color.g, color.b], i * 3);
	}
	const index = [];
	for (let r = 0; r < rows - 1; r++) {
		for (let c = 0; c < cols - 1; c++) {
			const a = r * cols + c, b = a + 1, d = a + cols, e = d + 1;
			if (valid[a] && valid[b] && valid[d] && valid[e]) index.push(a, d, b, b, d, e);
		}
	}
	if (!index.length) return null;
	const geometry = new THREE.BufferGeometry();
	geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
	geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
	geometry.setIndex(index);
	geometry.computeVertexNormals();
	const mesh = new THREE.Mesh(geometry, new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.95, metalness: 0, side: THREE.DoubleSide }));
	mesh.receiveShadow = true;
	return mesh;
}

/** Ground height at (x, z) from terrain data, or null outside it or where it has holes. */
function heightSampler(t) {
	if (!t || !t.heights) return () => null;
	const bytes = atob(t.heights);
	return (x, z) => {
		const c = Math.round((x - t.minX) / t.step), r = Math.round((z - t.minZ) / t.step);
		if (c < 0 || r < 0 || c >= t.width || r >= t.height) return null;
		const i = (r * t.width + c) * 2;
		const value = bytes.charCodeAt(i) | (bytes.charCodeAt(i + 1) << 8);
		return value === 65535 ? null : t.minY + (value / 65534) * (t.maxY - t.minY);
	};
}

/** Decode base64 to bytes */
function bytesOf(base64) {
	const text = atob(base64);
	const bytes = new Uint8Array(text.length);
	for (let i = 0; i < text.length; i++) bytes[i] = text.charCodeAt(i);
	return bytes;
}

// A size x size BGRA map from the .raw file as a texture (stored as the game reads it: no colour space conversion).
// A missing map (size 0) is plain white.
function bgraTexture(base64, size) {
	if (!size) {
		const white = new THREE.DataTexture(new Uint8Array([255, 255, 255, 255]), 1, 1, THREE.RGBAFormat);
		white.needsUpdate = true;
		return white;
	}
	const bgra = bytesOf(base64);
	const rgba = new Uint8Array(size * size * 4);
	for (let i = 0; i < size * size; i++) {
		rgba[i * 4] = bgra[i * 4 + 2]; rgba[i * 4 + 1] = bgra[i * 4 + 1]; rgba[i * 4 + 2] = bgra[i * 4]; rgba[i * 4 + 3] = bgra[i * 4 + 3];
	}
	const texture = new THREE.DataTexture(rgba, size, size, THREE.RGBAFormat);
	texture.magFilter = THREE.LinearFilter;
	texture.minFilter = THREE.LinearFilter;
	texture.needsUpdate = true;
	return texture;
}

// The client's terrain shader (res/shaders/TerrainDiffuse.fx, TiledDetailDiffuse_4_PS): four tiled textures blended
// by the blend map's red, green and blue, times twice the colour map, lit by the sun, darkened by the blend's alpha.
// The other looks show one layer of the terrain file: the colour map alone, the blend map's weights, or the scene of
// each cell (the scene map, coloured by a 256 x 1 palette) over the lit colour map.
export const TERRAIN_LOOKS = { textured: 0, colorMap: 1, blendMap: 2, scenes: 3 };
const TERRAIN_VERTEX = `
varying vec2 vUv;
varying vec3 vNormal;
varying vec3 vWorld;
void main() {
	vUv = uv;
	vNormal = normal;
	vWorld = (modelMatrix * vec4(position, 1.0)).xyz;
	gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}`;
const TERRAIN_FRAGMENT = `
uniform sampler2D texture1, texture2, texture3, texture4, blendMap, colorMap, sceneMap, scenePalette;
uniform vec3 lightDirection;
// The zone's lights as the game's TerrainDiffuse.fx gets them (scenery.js gameLights), when gameLightOn
uniform vec3 gameLightColor, gameAmbient, gameLightVec;
uniform float gameLightOn;
// ... and its fog, when the view turns it on (game-shaders.js)
uniform vec3 gameFogColor;
uniform float gameFogNear, gameFogFar, gameFogOn;
varying vec3 vWorld;
uniform int look;
uniform float sceneSize;
varying vec2 vUv;
varying vec3 vNormal;
void main() {
	float light = max(0.0, dot(normalize(vNormal), lightDirection)) * 0.75 + 0.4;
	vec4 blend = texture2D(blendMap, vUv);
	vec3 tint = texture2D(colorMap, vUv).rgb * 2.0;
	if (look == 1) { gl_FragColor = vec4(min(tint * 0.5, 1.0) * light, 1.0); return; }
	if (look == 2) { gl_FragColor = vec4(blend.rgb * light, 1.0); return; }
	if (look == 3) {
		// As the server finds a position's scene: the cell at floor(uv * (size - 1))
		vec2 cell = (floor(vUv * (sceneSize - 1.0) + 0.0001) + 0.5) / sceneSize;
		float id = floor(texture2D(sceneMap, cell).r * 255.0 + 0.5);
		vec3 scene = texture2D(scenePalette, vec2((id + 0.5) / 256.0, 0.5)).rgb;
		gl_FragColor = vec4(mix(min(tint * 0.5, 1.0), scene, 0.7) * light, 1.0);
		return;
	}
	vec2 tiled = vUv * 4.0;
	vec4 color = texture2D(texture1, tiled);
	color = mix(color, texture2D(texture2, tiled), blend.r);
	color = mix(color, texture2D(texture3, tiled), blend.g);
	color = mix(color, texture2D(texture4, tiled), blend.b);
	if (gameLightOn > 0.5) {
		// TiledDetailDiffuse_4_PS: textures * diffuse map * 2 * (sun * N.L + ambient, clamped as a vertex color), * blend alpha
		vec3 lit = clamp(gameLightColor * max(0.0, dot(normalize(vNormal), gameLightVec)) + gameAmbient, 0.0, 1.0);
		vec3 terrain = min(color.rgb * tint * lit * blend.a, 1.0);
		float fog = gameFogOn * clamp((length(vWorld - cameraPosition) - gameFogNear) / max(1.0, gameFogFar - gameFogNear), 0.0, 1.0);
		gl_FragColor = vec4(mix(terrain, gameFogColor, fog), 1.0);
		return;
	}
	gl_FragColor = vec4(min(color.rgb * tint, 1.0) * light * blend.a, 1.0);
}`;

// One chunk's scene map (size x size scene ids, rows along x like the other maps) as a texture read without filtering
function sceneTexture(base64, size) {
	const texture = size ? new THREE.DataTexture(bytesOf(base64), size, size, THREE.RedFormat, THREE.UnsignedByteType)
		: new THREE.DataTexture(new Uint8Array([0]), 1, 1, THREE.RedFormat, THREE.UnsignedByteType);
	texture.magFilter = texture.minFilter = THREE.NearestFilter;
	texture.unpackAlignment = 1;
	texture.needsUpdate = true;
	return texture;
}

/**
 * The zone's whole terrain, every chunk, the way the game draws it (from /api/properties/:id/terrain_chunks).
 * Returns the group, a function giving the ground height at (x, z), and setLook(look, layers): switch to one of
 * TERRAIN_LOOKS; the scene look needs the zone's terrain layers (/api/world3d/:zone/terrain_layers).
 */
export function buildTerrainChunks(data, loadTexture, lightDirection, gameLights = null) {
	const group = new THREE.Group();
	const grids = [];
	const materials = [];
	for (const chunk of data.chunks) {
		const { width: w, height: h, scale } = chunk;
		const heights = new Float32Array(bytesOf(chunk.heights).buffer);
		const positions = new Float32Array(w * h * 3), uvs = new Float32Array(w * h * 2);
		// heights[w * i + j] is at x = chunk.x + i * scale, z = chunk.z + j * scale; the maps run the same way
		for (let i = 0; i < w; i++) {
			for (let j = 0; j < h; j++) {
				const v = i * h + j;
				positions.set([chunk.x + i * scale, heights[w * i + j], chunk.z + j * scale], v * 3);
				uvs.set([j / (h - 1), i / (w - 1)], v * 2);
			}
		}
		const index = [];
		for (let i = 0; i < w - 1; i++) {
			for (let j = 0; j < h - 1; j++) {
				const a = i * h + j, b = a + 1, c = a + h, d = c + 1;
				index.push(a, b, c, b, d, c);
			}
		}
		const geometry = new THREE.BufferGeometry();
		geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
		geometry.setAttribute('uv', new THREE.BufferAttribute(uvs, 2));
		geometry.setIndex(index);
		geometry.computeVertexNormals();
		const material = new THREE.ShaderMaterial({
			vertexShader: TERRAIN_VERTEX, fragmentShader: TERRAIN_FRAGMENT, side: THREE.DoubleSide,
			uniforms: {
				texture1: { value: loadTexture(chunk.textures[0]) }, texture2: { value: loadTexture(chunk.textures[1]) },
				texture3: { value: loadTexture(chunk.textures[2]) }, texture4: { value: loadTexture(chunk.textures[3]) },
				blendMap: { value: bgraTexture(chunk.blend, chunk.blendSize) }, colorMap: { value: bgraTexture(chunk.color, chunk.colorSize) },
				sceneMap: { value: null }, scenePalette: { value: null }, sceneSize: { value: 1 },
				lightDirection: { value: lightDirection }, look: { value: 0 },
				// Shared with the scenery, so the terrain follows its lighting (and its blends between scenes)
				gameLightColor: gameLights ? gameLights.gameLightColor : { value: new THREE.Vector3() },
				gameAmbient: gameLights ? gameLights.gameAmbient : { value: new THREE.Vector3() },
				gameLightVec: gameLights ? gameLights.gameLightVec : { value: new THREE.Vector3(0, 1, 0) },
				gameLightOn: gameLights ? gameLights.gameLightOn : { value: 0 },
				gameFogColor: gameLights ? gameLights.gameFogColor : { value: new THREE.Vector3(1, 1, 1) },
				gameFogNear: gameLights ? gameLights.gameFogNear : { value: 0 }, gameFogFar: gameLights ? gameLights.gameFogFar : { value: 0 },
				gameFogOn: gameLights ? gameLights.gameFogOn : { value: 0 }
			}
		});
		const mesh = new THREE.Mesh(geometry, material);
		mesh.receiveShadow = true;
		group.add(mesh);
		materials.push(material);
		grids.push({ x: chunk.x, z: chunk.z, w, h, scale, heights });
	}
	const heightAt = (x, z) => {
		for (const g of grids) {
			const i = Math.round((x - g.x) / g.scale), j = Math.round((z - g.z) / g.scale);
			if (i >= 0 && j >= 0 && i < g.w && j < g.h) return g.heights[g.w * i + j];
		}
		return null;
	};
	let palette = null;
	const setLook = (look, layers) => {
		if (look === TERRAIN_LOOKS.scenes && layers && !palette) {
			const colors = new Uint8Array(256 * 4).fill(128);
			for (const scene of layers.scenes) colors.set([...scene.color, 255], scene.id * 4);
			palette = new THREE.DataTexture(colors, 256, 1, THREE.RGBAFormat);
			palette.magFilter = palette.minFilter = THREE.NearestFilter;
			palette.needsUpdate = true;
			layers.chunks.forEach((c, i) => {
				if (!materials[i]) return;
				materials[i].uniforms.sceneMap.value = sceneTexture(c.scenes, c.sceneSize);
				materials[i].uniforms.sceneSize.value = Math.max(1, c.sceneSize);
			});
		}
		// The shared palette isn't disposed with each chunk's uniforms
		if (palette) palette.userData.shared = true;
		const usable = look !== TERRAIN_LOOKS.scenes || palette;
		for (const material of materials) {
			material.uniforms.look.value = usable ? look : 0;
			material.uniforms.scenePalette.value = palette;
		}
	};
	const dispose = () => { if (palette) palette.dispose(); };
	return { group, heightAt, setLook, dispose };
}

/**
 * A property's build area: its outline drawn on the ground as a line with a low see-through wall, following the
 * terrain when there is some. The zone file's outline points are flat (y is 0), so heights come from the terrain,
 * else the lowest model.
 */
function buildBoundary(areas, heightAt, fallbackY) {
	const group = new THREE.Group();
	const WALL = 2, STEP = 1;
	for (const area of areas) {
		if (!area.outline || area.outline.length < 3) continue;
		const line = [], wall = [];
		const outline = area.outline.concat([area.outline[0]]);
		for (let i = 0; i < outline.length - 1; i++) {
			const [ax, , az] = outline[i], [bx, , bz] = outline[i + 1];
			const steps = Math.max(1, Math.ceil(Math.hypot(bx - ax, bz - az) / STEP));
			for (let s = 0; s < steps; s++) {
				const x = ax + (bx - ax) * s / steps, z = az + (bz - az) * s / steps;
				const y = (heightAt(x, z) ?? fallbackY) + 0.1;
				line.push(x, y, z);
			}
		}
		line.push(line[0], line[1], line[2]);
		for (let i = 0; i < line.length / 3 - 1; i++) {
			const a = line.slice(i * 3, i * 3 + 3), b = line.slice(i * 3 + 3, i * 3 + 6);
			wall.push(...a, ...b, a[0], a[1] + WALL, a[2], ...b, b[0], b[1] + WALL, b[2], a[0], a[1] + WALL, a[2]);
		}
		const lineGeometry = new THREE.BufferGeometry();
		lineGeometry.setAttribute('position', new THREE.Float32BufferAttribute(line, 3));
		group.add(new THREE.Line(lineGeometry, new THREE.LineBasicMaterial({ color: 0x33d6ff })));
		const wallGeometry = new THREE.BufferGeometry();
		wallGeometry.setAttribute('position', new THREE.Float32BufferAttribute(wall, 3));
		group.add(new THREE.Mesh(wallGeometry, new THREE.MeshBasicMaterial({ color: 0x33d6ff, transparent: true, opacity: 0.28, side: THREE.DoubleSide, depthWrite: false })));
	}
	return group;
}

// brickUrl(lod, design): where brick geometry comes from (default /api/bricks/:lod/:design; the showcase has its own)
export function createViewer(container, { onProgress, onSelect, onTick, brickUrl } = {}) {
	const renderer = new THREE.WebGLRenderer({ antialias: true });
	renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
	renderer.toneMapping = THREE.ACESFilmicToneMapping;
	renderer.shadowMap.enabled = true;
	renderer.shadowMap.type = THREE.PCFSoftShadowMap;
	container.textContent = '';
	container.style.position = 'relative';
	container.appendChild(renderer.domElement);
	// Speech bubbles (behavior chat) float over the canvas
	const overlay = document.createElement('div');
	overlay.style.cssText = 'position:absolute;inset:0;pointer-events:none;overflow:hidden';
	container.appendChild(overlay);

	const scene = new THREE.Scene();
	scene.background = new THREE.Color(0x1e2126);
	const pmrem = new THREE.PMREMGenerator(renderer);
	scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;

	const sun = new THREE.DirectionalLight(0xffffff, 1.6);
	sun.castShadow = true;
	sun.shadow.mapSize.set(2048, 2048);
	sun.shadow.bias = -0.0005;
	scene.add(sun, sun.target);

	const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 10000);
	let framedNear = 0.1; // the near plane framing set
	const controls = new OrbitControls(camera, renderer.domElement);
	controls.enableDamping = true;

	const root = new THREE.Group();
	scene.add(root);
	const ground = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), new THREE.ShadowMaterial({ opacity: 0.35 }));
	ground.rotation.x = -Math.PI / 2;
	ground.receiveShadow = true;
	scene.add(ground);
	let grid = null;

	const selection = new THREE.Box3Helper(new THREE.Box3(), 0xffc107);
	selection.visible = false;
	scene.add(selection);

	let models = [];       // [{id, name, lot, box: Box3, instances: [{mesh, index, base}], offset, visible}]
	let terrain = null;
	let boundary = null;
	let groundHeight = () => null; // from the chunk terrain, for the build area line

	// Remove whichever terrain is shown (the height-shaded mesh or the game's chunks) and free it
	function removeTerrain() {
		if (!terrain) return;
		scene.remove(terrain);
		terrain.traverse((o) => {
			if (o.geometry) o.geometry.dispose();
			if (!o.material) return;
			for (const u of Object.values(o.material.uniforms || {})) if (u.value && u.value.isTexture && !u.value.userData.shared) u.value.dispose();
			o.material.dispose();
		});
		terrain = null;
		groundHeight = () => null;
	}
	const bubbles = new Map(); // modelIndex -> {el, until}
	let meshes = [];       // InstancedMeshes, each with userData.modelIndex[instanceId]
	let sceneBox = new THREE.Box3();
	let running = true;

	function resize() {
		const width = container.clientWidth;
		const height = container.clientHeight || 520;
		camera.aspect = width / height;
		camera.updateProjectionMatrix();
		renderer.setSize(width, height);
	}
	const observer = new ResizeObserver(resize);
	// The wheel only zooms the view, never scrolls the page
	container.addEventListener('wheel', (e) => e.preventDefault(), { passive: false });
	observer.observe(container);
	resize();

	// minSize keeps the camera back from small models so it doesn't end up inside whatever is around them
	function frame(box, minSize = 4) {
		if (box.isEmpty()) return;
		const center = box.getCenter(new THREE.Vector3());
		const size = Math.max(box.getSize(new THREE.Vector3()).length(), minSize);
		controls.target.copy(center);
		camera.position.copy(center).add(new THREE.Vector3(size * 0.6, size * 0.75, size * 0.6));
		camera.near = framedNear = size / 500;
		camera.far = size * 50;
		camera.updateProjectionMatrix();
	}

	function clear() {
		for (const mesh of meshes) {
			root.remove(mesh);
			if (mesh.userData.owned) { mesh.geometry.dispose(); mesh.material.dispose(); }
			mesh.dispose();
		}
		meshes = [];
		models = [];
		selection.visible = false;
		for (const bubble of bubbles.values()) bubble.el.remove();
		bubbles.clear();
		if (grid) { scene.remove(grid); grid.geometry.dispose(); grid = null; }
	}

	/**
	 * Load property models: [{id, name, lot, position: [x,y,z], rotation: [x,y,z,w]}], each from `url` when given (else
	 * its property model LXFML). A model with `meshUrl` is drawn from the model the UGC server made of it instead, or
	 * from its LXFML when that can't be had (model.generated says which it is drawn from).
	 */
	async function load(list, lod = 2) {
		clear();
		const batches = new Map(); // "design/part/material" -> {geometry, material, matrices: [], modelIndex: []}
		const boxes = list.map(() => new THREE.Box3());
		const generated = new Set(); // indexes of the models drawn from the UGC server's model
		let loaded = 0;

		await Promise.all(list.map(async (model, modelIndex) => {
			try {
				if (model.meshUrl) {
					try {
						const placement = new THREE.Matrix4().compose(new THREE.Vector3(...model.position), new THREE.Quaternion(...model.rotation), new THREE.Vector3(1, 1, 1));
						const parts = await loadGeneratedModel(model.meshUrl);
						parts.forEach((part, index) => {
							batches.set(`generated/${modelIndex}/${index}`, { ...part, owned: true, matrices: [placement], modelIndex: [modelIndex] });
							boxes[modelIndex].union(part.geometry.boundingBox.clone().applyMatrix4(placement));
						});
						generated.add(modelIndex);
						return;
					} catch (e) {
						// Not made yet, or the UGC server is away: its LXFML instead
					}
				}
				const response = await fetch(model.url || `/api/property_models/${model.id}/lxfml`, { credentials: 'same-origin' });
				if (!response.ok) return;
				const parts = parseLxfml(await response.text());
				const placement = new THREE.Matrix4().compose(
					new THREE.Vector3(...model.position),
					new THREE.Quaternion(...model.rotation),
					new THREE.Vector3(1, 1, 1));

				await Promise.all(parts.map(async (part) => {
					const geometries = await loadDesign(part.designId, lod, brickUrl);
					const world = placement.clone().multiply(part.matrix);
					geometries.forEach((geometry, index) => {
						const materialId = part.materials[index] || part.materials[0];
						const key = `${part.designId}/${index}/${materialId}`;
						if (!batches.has(key)) batches.set(key, { geometry, material: material(materialId), matrices: [], modelIndex: [] });
						const batch = batches.get(key);
						batch.matrices.push(world);
						batch.modelIndex.push(modelIndex);
						boxes[modelIndex].union(geometry.boundingBox.clone().applyMatrix4(world));
					});
				}));
			} finally {
				loaded++;
				if (onProgress) onProgress(loaded, list.length);
			}
		}));

		for (const batch of batches.values()) {
			const mesh = new THREE.InstancedMesh(batch.geometry, batch.material, batch.matrices.length);
			batch.matrices.forEach((matrix, i) => mesh.setMatrixAt(i, matrix));
			mesh.instanceMatrix.needsUpdate = true;
			mesh.castShadow = true;
			mesh.receiveShadow = true;
			mesh.userData.modelIndex = batch.modelIndex;
			mesh.userData.owned = !!batch.owned; // its geometry and material are its own, not shared bricks
			mesh.computeBoundingSphere();
			root.add(mesh);
			meshes.push(mesh);
		}

		models = list.map((model, i) => ({ ...model, generated: generated.has(i), box: boxes[i], instances: [], offset: new THREE.Vector3(), visible: true }));
		for (const mesh of meshes) {
			mesh.userData.modelIndex.forEach((modelIndex, index) => {
				const base = new THREE.Matrix4();
				mesh.getMatrixAt(index, base);
				models[modelIndex].instances.push({ mesh, index, base });
			});
		}
		sceneBox = new THREE.Box3();
		boxes.forEach((box) => { if (!box.isEmpty()) sceneBox.union(box); });
		if (sceneBox.isEmpty()) return;

		// Ground, grid and shadow camera sized to the scene
		const size = sceneBox.getSize(new THREE.Vector3());
		const center = sceneBox.getCenter(new THREE.Vector3());
		const extent = Math.max(size.x, size.z, 8) * 1.5;
		ground.scale.set(extent, extent, 1);
		ground.position.set(center.x, sceneBox.min.y - 0.01, center.z);
		grid = new THREE.GridHelper(extent, Math.max(8, Math.round(extent / 3.2)), 0x555a66, 0x33363d);
		grid.position.copy(ground.position);
		scene.add(grid);

		const radius = size.length() / 2 + 2;
		sun.position.copy(center).add(new THREE.Vector3(radius, radius * 2, radius * 0.6));
		sun.target.position.copy(center);
		Object.assign(sun.shadow.camera, { left: -radius, right: radius, top: radius, bottom: -radius, near: 0.1, far: radius * 5 });
		sun.shadow.camera.updateProjectionMatrix();

		frame(sceneBox);
	}

	let selectedModel = null;
	function select(index) {
		const model = models[index];
		selectedModel = model || null;
		if (!model || model.box.isEmpty()) {
			selection.visible = false;
			if (onSelect) onSelect(null);
			return;
		}
		selection.box.copy(model.box).translate(model.offset).expandByScalar(0.05);
		selection.visible = true;
		if (onSelect) onSelect(model, index);
	}

	// Click selects, double-click also focuses; ignore clicks that were really drags
	const raycaster = new THREE.Raycaster();
	let downAt = null;
	function pick(event) {
		const rect = renderer.domElement.getBoundingClientRect();
		const pointer = new THREE.Vector2(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1);
		raycaster.setFromCamera(pointer, camera);
		const hit = raycaster.intersectObjects(meshes, false)[0];
		return hit ? hit.object.userData.modelIndex[hit.instanceId] : -1;
	}
	renderer.domElement.addEventListener('pointerdown', (e) => { downAt = [e.clientX, e.clientY]; });
	renderer.domElement.addEventListener('click', (e) => {
		if (!downAt || Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]) > 4) return;
		select(pick(e));
	});
	renderer.domElement.addEventListener('dblclick', (e) => {
		const index = pick(e);
		if (index >= 0) { select(index); frame(models[index].box.clone().translate(models[index].offset), 16); }
	});

	// Move or hide one model (behavior playback): its bricks are instances spread over several meshes
	const hidden = new THREE.Matrix4().makeScale(0, 0, 0);
	const moved = new THREE.Matrix4();
	function place(index) {
		const model = models[index];
		if (!model) return;
		const touched = new Set();
		for (const instance of model.instances) {
			if (!model.visible) instance.mesh.setMatrixAt(instance.index, hidden);
			else instance.mesh.setMatrixAt(instance.index, moved.makeTranslation(model.offset.x, model.offset.y, model.offset.z).multiply(instance.base));
			touched.add(instance.mesh);
		}
		for (const mesh of touched) { mesh.instanceMatrix.needsUpdate = true; mesh.computeBoundingSphere(); }
	}

	const projected = new THREE.Vector3();
	function updateBubbles(now) {
		const rect = renderer.domElement.getBoundingClientRect();
		for (const [index, bubble] of bubbles) {
			const model = models[index];
			if (!model || now > bubble.until) { bubble.el.remove(); bubbles.delete(index); continue; }
			model.box.getCenter(projected);
			projected.y = model.box.max.y;
			projected.add(model.offset).project(camera);
			const visible = projected.z < 1 && model.visible;
			bubble.el.style.display = visible ? '' : 'none';
			bubble.el.style.left = ((projected.x + 1) / 2 * rect.width) + 'px';
			bubble.el.style.top = ((1 - projected.y) / 2 * rect.height) + 'px';
		}
	}

	let lastTime = performance.now();
	(function animate() {
		if (!running) return;
		requestAnimationFrame(animate);
		const now = performance.now();
		const dt = Math.min((now - lastTime) / 1000, 0.1);
		lastTime = now;
		if (onTick) onTick(dt);
		for (const glitter of glitterMaterials) glitter.update(now / 1000);
		updateBubbles(now);
		controls.update();
		// The near plane follows how far out the camera is (depth precision for the scenery far away), but never
		// past what framing the models set, so close-ups of small models keep working
		const near = Math.min(nearPlaneFor(camera.position.distanceTo(controls.target)), framedNear * 20);
		if (Math.abs(near - camera.near) / camera.near > 0.15) {
			camera.near = near;
			camera.updateProjectionMatrix();
		}
		renderer.render(scene, camera);
	})();

	return {
		load,
		select(id) {
			const index = models.findIndex((m) => m.id === id);
			select(index);
			if (index >= 0) frame(models[index].box.clone().translate(models[index].offset), 16);
		},
		/** Frame everything: the models and, when shown, the build area. */
		resetView() {
			selection.visible = false;
			const box = sceneBox.clone();
			if (boundary) box.union(new THREE.Box3().setFromObject(boundary));
			frame(box);
		},
		setShadows(enabled) { sun.castShadow = enabled; ground.visible = enabled && !terrain; },
		/** Show or hide the placed models (the selection box goes with them). */
		setModelsVisible(visible) { root.visible = visible; if (!visible) selection.visible = false; },
		/** Show the zone's terrain under the models (null removes it). Returns false if nothing is near the models. */
		setTerrain(data) {
			removeTerrain();
			if (data && !sceneBox.isEmpty()) {
				const size = sceneBox.getSize(new THREE.Vector3());
				terrain = buildTerrain(data, sceneBox, Math.max(40, Math.max(size.x, size.z) * 0.75));
				if (terrain) scene.add(terrain);
			}
			ground.visible = sun.castShadow && !terrain;
			if (grid) grid.visible = !terrain;
			return !!terrain || !data;
		},
		/**
		 * The zone's whole terrain as the game draws it (null removes it): data from /api/properties/:id/terrain_chunks,
		 * textures from textureUrl(id).
		 */
		setTerrainChunks(data, textureUrl, gameLights = null) {
			removeTerrain();
			if (data && data.chunks && data.chunks.length) {
				const loader = new THREE.TextureLoader();
				const textures = new Map();
				const loadTexture = (id) => {
					if (!textures.has(id)) {
						const texture = loader.load(textureUrl(id), undefined, undefined, () => {});
						texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
						texture.anisotropy = 8;
						texture.userData.shared = true;
						textures.set(id, texture);
					}
					return textures.get(id);
				};
				const sunDirection = sun.position.clone().sub(sun.target.position).normalize();
				const built = buildTerrainChunks(data, loadTexture, sunDirection, gameLights);
				terrain = built.group;
				groundHeight = built.heightAt;
				scene.add(terrain);
			}
			ground.visible = sun.castShadow && !terrain;
			if (grid) grid.visible = !terrain;
		},
		/** Show a property's build areas ([{outline: [[x, y, z]...]}]; null removes them). Follows the terrain's heights. */
		setBoundary(areas, terrainData) {
			if (boundary) {
				scene.remove(boundary);
				boundary.traverse((o) => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); });
				boundary = null;
			}
			if (!areas || !areas.length) return;
			const heightAt = terrainData ? heightSampler(terrainData) : groundHeight;
			boundary = buildBoundary(areas, heightAt, sceneBox.isEmpty() ? 0 : sceneBox.min.y);
			scene.add(boundary);
		},
		models() { return models; },
		/** The view's three.js parts, for layers drawn by other modules (the zone's scenery). */
		three() { return { scene, camera, renderer, controls }; },
		/** Offset a model from where it was placed, and show or hide it (smashed). */
		setModelState(index, offset, visible) {
			const model = models[index];
			if (!model) return;
			model.offset.copy(offset);
			model.visible = visible;
			place(index);
			if (selection.visible && model === selectedModel) selection.box.copy(model.box).translate(model.offset).expandByScalar(0.05);
		},
		/** A speech bubble over a model for a few seconds. */
		say(index, text, seconds = 4) {
			let bubble = bubbles.get(index);
			if (!bubble) {
				const el = document.createElement('div');
				el.style.cssText = 'position:absolute;transform:translate(-50%,-120%);max-width:14rem;padding:.25rem .5rem;border-radius:.5rem;' +
					'background:rgba(255,255,255,.92);color:#111;font-size:.8rem;white-space:pre-wrap;box-shadow:0 1px 4px rgba(0,0,0,.4)';
				overlay.appendChild(el);
				bubble = { el, until: 0 };
				bubbles.set(index, bubble);
			}
			bubble.el.textContent = text;
			bubble.until = performance.now() + seconds * 1000;
		},
		dispose() {
			running = false;
			observer.disconnect();
			clear();
			pmrem.dispose();
			renderer.dispose();
		}
	};
}
