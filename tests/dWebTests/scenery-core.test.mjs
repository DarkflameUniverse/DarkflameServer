// The 3D views' shader lookups (static/js/scenery-core.js): which technique draws a mesh and what it uses.
// Run by ctest: node scenery-core.test.mjs <scenery-core.js> [NifFile.h]
import { pathToFileURL } from 'node:url';
import { readFileSync } from 'node:fs';

const [modulePath, nifHeader] = process.argv.slice(2);
const S = await import(pathToFileURL(modulePath).href);
let failures = 0;
const same = (actual, expected, what) => {
	if (JSON.stringify(actual) !== JSON.stringify(expected)) {
		failures++;
		console.error(`${what}: ${JSON.stringify(actual)} is not ${JSON.stringify(expected)}`);
	}
};

// A manifest as Scenery.cpp writes it: "techniques" from NifFile::TechniquesJson
const L = S.SHADER_LOOK, T = S.TECHNIQUE;
const manifest = {
	shaders: [38, 9999, -1, 33],
	shaderTags: { 1: 5, 30: 38, 25: 33, 2: 2 },
	techniques: {
		'-1': { family: 'fixed', look: 0, alpha: 'opacity', flags: 0 },
		5: { family: 'lego', look: 0, alpha: 'decal', flags: 0 },
		33: { family: 'basic', look: L.UNLIT | L.NO_TEXTURE, alpha: 'opacity', flags: T.ANIM_ALPHA },
		38: { family: 'basic', look: 0, alpha: 'opacity', flags: 0 }
	},
	multishader: 9999,
	defaultShader: 5,
	lighting: { ambient: [0.4, 0.6, 0.7], light: [1, 1, 1], lightVec: [0, 1, 0] }
};
const colored = { colors: new Uint8Array(4), vertexColors: 2 };
const pick = (look, keys) => Object.fromEntries(keys.map((k) => [k, look[k]]));
const BASICS = ['family', 'lit', 'texture', 'vertexColors', 'material', 'layers', 'metal', 'emissive'];

// A multishader part's tag names its shader; an unusable or missing tag is the LEGO shader
same(S.shaderOf(manifest, 1, { shaderTag: 30 }), 38, 'tagged part');
same(S.shaderOf(manifest, 1, { shaderTag: 2 }), 5, 'unusable tag');
same(S.shaderOf(manifest, 1, { shaderTag: -1 }), 5, 'untagged part');
same(S.shaderOf(manifest, 0, {}), 38, 'object shader');
same(S.shaderOf({}, 0, {}), null, 'no shaders in the manifest');
same(S.textureAlphaMode(manifest, 1, { shaderTag: 1 }), 'decal', 'LEGO part texture alpha');
same(S.textureAlphaMode(manifest, 1, { shaderTag: 30 }), 'opacity', 'Basic VC part texture alpha');

// Techniques: the manifest's table, fixed function without a shader, the LEGO shader for one it lacks
same(S.techniqueOf(manifest, 0, {}).family, 'basic', 'Basic VC technique');
same(S.techniqueOf(manifest, 2, {}).family, 'fixed', 'fixed function technique');
same(S.techniqueOf({ ...manifest, shaders: [77] }, 0, {}), { shader: 77, family: 'lego', look: 0, alpha: 'decal', flags: 0 }, 'unknown shader is LEGO');
same(S.techniqueOf({ ...manifest, technique: { family: 'flair', look: 0, alpha: 'opacity', flags: 0 } }, 0, {}).family, 'flair', 'manifest-wide technique');

// Lit, textured, vertex colors, no material colors: Basic VC
same(pick(S.gameLook(manifest, 1, { ...colored, shaderTag: 30 }), BASICS), { family: 'basic', lit: true, texture: true, vertexColors: true, material: false, layers: null, metal: null, emissive: false }, 'Basic VC');
// Vertex colors are read even when NiVertexColorProperty ignores them, but only if the mesh has some
same(S.gameLook(manifest, 0, { ...colored, vertexColors: 0 }).vertexColors, true, 'shader reads vertex colors');
same(S.gameLook(manifest, 0, {}).vertexColors, false, 'mesh without vertex colors');
same(pick(S.gameLook(manifest, 3, colored), BASICS), { family: 'basic', lit: false, texture: false, vertexColors: true, material: false, layers: null, metal: null, emissive: false }, 'Basic NL VC NT');
// Fixed function: NiVertexColorProperty and the material decide
same(pick(S.gameLook(manifest, 2, { ...colored, vertexColors: 0 }), BASICS), { family: 'fixed', lit: true, texture: true, vertexColors: false, material: true, layers: null, metal: null, emissive: false }, 'fixed function');
// Without the zone's lighting the viewer lights scenery itself
same(S.gameLook({ ...manifest, lighting: null }, 0, colored), null, 'no lighting');

// What the flags turn into: moving textures, both sides, blending, not drawn
const flagged = (flags, family = 'basic') => S.gameLook({ ...manifest, shaders: [1], techniques: { 1: { family, look: 0, alpha: 'opacity', flags } } }, 0, colored);
same(flagged(T.UV_ANIM).uvAnim, true, 'UV animation');
same(flagged(0).uvAnim, false, 'still texture');
same(flagged(T.DOUBLE_SIDED).doubleSided, true, 'AlphaAsAlpha both sides');
same([flagged(0).blend, flagged(T.BLEND).blend, flagged(T.ALPHA_TEST).blend, flagged(T.ADDITIVE).blend, flagged(T.NO_BLEND).blend], ['nif', 'blend', 'test', 'additive', 'opaque'], 'blend modes');
same(flagged(T.NOT_DRAWN).hidden, true, 'not drawn');
same(flagged(T.ANIM_ALPHA).flags & T.ANIM_ALPHA, T.ANIM_ALPHA, 'flags kept');

// Blending as the look and the mesh say
same(S.blendingOf(null, { blend: true, test: -1 }, true), { transparent: true, depthWrite: true, additive: false, alphaCutoff: 0, doubleSided: false }, 'NiAlphaProperty blend where see-through');
same(S.blendingOf(flagged(0), { blend: true, test: -1 }, false).transparent, false, 'blend on but nothing see-through');
same(S.blendingOf(flagged(T.BLEND), { blend: false, test: -1 }, false), { transparent: true, depthWrite: false, additive: false, alphaCutoff: 0, doubleSided: false }, 'technique blends');
same(S.blendingOf(flagged(T.ADDITIVE), { blend: false, test: -1 }, false).additive, true, 'additive');
same(S.blendingOf(flagged(T.ALPHA_TEST), { blend: true, test: -1 }, true), { transparent: false, depthWrite: true, additive: false, alphaCutoff: 0.5, doubleSided: false }, 'alpha test');
same(S.blendingOf(flagged(T.NO_BLEND), { blend: true, test: 128 }, true).transparent, false, 'opaque technique');
same(S.blendingOf(flagged(T.DOUBLE_SIDED), { blend: false, test: -1, doubleSided: false }, false).doubleSided, true, 'Cullmode none');

// Scene maps: the same terrain as ZoneScenesTests.FindsTheSceneUnderAPosition, as runs of [length, scene]
const runs = (bytes) => Buffer.from(bytes).toString('base64');
const map = S.decodeSceneMap({ chunks: [
	{ x: 0, z: 0, maxX: 64, maxZ: 64, size: 2, runs: runs([1, 1, 1, 2, 1, 3, 1, 255]) },
	{ x: 64, z: 0, maxX: 128, maxZ: 64, size: 1, runs: runs([1, 7]) }
] });
same([[1, 1], [1, 17], [17, 1], [17, 17], [100, 10], [-50, -50], [500, 10]].map(([x, z]) => S.sceneAt(map, x, z)), [1, 2, 3, 0, 7, 1, 7], 'scene at');
same(S.sceneAt(null, 1, 1), S.GLOBAL_SCENE, 'no scene map');
const scenes = [{ id: 0, neighbours: [] }, { id: 1, neighbours: [2] }, { id: 2, neighbours: [1, 3] }, { id: 3, neighbours: [2] }];
same([...S.loadedScenes(scenes, 2)].sort(), [0, 1, 2, 3], 'loaded around 2');
same([...S.loadedScenes(scenes, 0)], [0], 'loaded in the global scene');
// A run longer than the map stops at its end
same(S.decodeSceneMap({ chunks: [{ x: 0, z: 0, maxX: 1, maxZ: 1, size: 1, runs: runs([9, 4]) }] }).chunks[0].cells.length, 1, 'runs clipped');

// Two layer shaders
const layered = { ...manifest, shaders: [106, 107], techniques: { 106: { family: 'basic', look: L.TWO_LAYERS_BLENDED, alpha: 'opacity', flags: T.UV_ANIM }, 107: { family: 'basic', look: L.TWO_LAYERS_ADDED, alpha: 'opacity', flags: T.UV_ANIM } } };
same(S.gameLook(layered, 0, colored).layers, 'blended', 'two layers blended');
same(S.gameLook(layered, 1, colored).layers, 'added', 'two layers added');
same(S.gameLook(manifest, 0, colored).layers, null, 'one layer');

// A player model's metal and glow groups (UGC server shader settings): Polished Metal, Brushed Steel, LEGO-Emissive
const shiny = { ...manifest, shaders: [9999], shaderTags: { 1: 5, 88: 98, 89: 99, 46: 53 },
	techniques: { ...manifest.techniques, 98: { family: 'metal', look: L.REFLECTIVE, alpha: 'opacity', flags: 0 }, 99: { family: 'metal', look: L.REFLECTIVE | L.BRUSHED, alpha: 'opacity', flags: 0 },
		53: { family: 'lego', look: L.EMISSIVE, alpha: 'opacity', flags: 0 } } };
same(S.gameLook(shiny, 0, { ...colored, shaderTag: 88 }).metal, 'polished', 'polished metal');
same(S.gameLook(shiny, 0, { ...colored, shaderTag: 89 }).metal, 'brushed', 'brushed steel');
same(S.gameLook(shiny, 0, { ...colored, shaderTag: 89 }).family, 'metal', 'metal family');
same(S.gameLook(shiny, 0, { ...colored, shaderTag: 46 }).emissive, true, 'emissive');
same(S.gameLook(shiny, 0, { ...colored, shaderTag: 1 }).metal, null, 'plastic');
same([S.metalOf(0), S.metalOf(L.REFLECTIVE), S.metalOf(L.REFLECTIVE | L.BRUSHED)], [null, 'polished', 'brushed'], 'metal of look bits');

// Environment cubes: a DXT1 cube of six 4x4 faces, one color each, made RGBA
const cube = new Uint8Array(128 + 6 * 8);
const header = new DataView(cube.buffer);
header.setUint32(0, 0x20534444, true);
header.setUint32(12, 4, true); header.setUint32(16, 4, true); header.setUint32(28, 1, true);
header.setUint32(80, 0x4, true); header.setUint32(84, 0x31545844, true); header.setUint32(112, 0xfe00, true);
const faceColors = [0xf800, 0x07e0, 0x001f, 0xffff, 0x0000, 0x8410];
faceColors.forEach((c, f) => { const at = 128 + f * 8; cube[at] = c & 255; cube[at + 1] = c >> 8; cube[at + 2] = c & 255; cube[at + 3] = c >> 8; });
const parsed = S.parseDdsCube(cube.buffer, 256);
same(parsed.faces.length, 6, 'six faces');
same([...parsed.faces[0].data.slice(0, 4)], [255, 0, 0, 255], '+X red');
same([...parsed.faces[2].data.slice(0, 4)], [0, 0, 255, 255], '+Y blue');
same([parsed.faces[5].width, parsed.faces[5].height], [4, 4], 'face size');
same(S.parseDdsCube(cube.buffer, 2).faces[0].width, 2, 'faces made smaller');
same(S.parseDdsCube(cube.buffer, 256, true), null, 'a cube is no plain texture');
header.setUint32(112, 0, true);
same(S.parseDdsCube(cube.buffer, 256), null, 'a plain texture is no cube');
same(S.parseDdsCube(cube.buffer, 256, true).width, 4, 'plain texture');

// The flag and look bits are the server's (NifFile.h eTechniqueFlag, eShaderLook)
if (nifHeader) {
	const text = readFileSync(nifHeader, 'utf8');
	const bitsOf = (name) => {
		const block = text.slice(text.indexOf('enum ' + name)).split('};')[0];
		return Object.fromEntries([...block.matchAll(/^\s*([A-Z_]+) = (\d+)/gm)].map((m) => [m[1], Number(m[2])]));
	};
	same(bitsOf('eTechniqueFlag'), T, 'TECHNIQUE matches eTechniqueFlag');
	same(bitsOf('eShaderLook'), L, 'SHADER_LOOK matches eShaderLook');
}

// The near plane grows with the distance, within limits
same([S.nearPlaneFor(10), S.nearPlaneFor(2000), S.nearPlaneFor(100000)], [0.5, 5, 20], 'near plane');

if (failures) {
	console.error(`${failures} failed`);
	process.exit(1);
}
console.log('scenery-core: all passed');
