// The 3D views' shader lookups (static/js/scenery-core.js): which shader draws a mesh and what it uses.
// Run by ctest: node scenery-core.test.mjs <scenery-core.js>
import { pathToFileURL } from 'node:url';

const [modulePath] = process.argv.slice(2);
const S = await import(pathToFileURL(modulePath).href);
let failures = 0;
const same = (actual, expected, what) => {
	if (JSON.stringify(actual) !== JSON.stringify(expected)) {
		failures++;
		console.error(`${what}: ${JSON.stringify(actual)} is not ${JSON.stringify(expected)}`);
	}
};

const manifest = {
	shaders: [38, 9999, -1, 33],
	shaderTags: { 1: 5, 30: 38, 25: 33, 2: 2 },
	textureAlpha: { 5: 'decal' },
	shaderLooks: { 33: S.SHADER_LOOK.UNLIT | S.SHADER_LOOK.NO_TEXTURE },
	multishader: 9999,
	defaultShader: 5,
	lighting: { ambient: [0.4, 0.6, 0.7], light: [1, 1, 1], lightVec: [0, 1, 0] }
};
const colored = { colors: new Uint8Array(4), vertexColors: 2 };

// A multishader part's tag names its shader; an unusable or missing tag is the LEGO shader
same(S.shaderOf(manifest, 1, { shaderTag: 30 }), 38, 'tagged part');
same(S.shaderOf(manifest, 1, { shaderTag: 2 }), 5, 'unusable tag');
same(S.shaderOf(manifest, 1, { shaderTag: -1 }), 5, 'untagged part');
same(S.shaderOf(manifest, 0, {}), 38, 'object shader');
same(S.shaderOf({}, 0, {}), null, 'no shaders in the manifest');
same(S.textureAlphaMode(manifest, 1, { shaderTag: 1 }), 'decal', 'LEGO part texture alpha');
same(S.textureAlphaMode(manifest, 1, { shaderTag: 30 }), 'opacity', 'Basic VC part texture alpha');

// Lit, textured, vertex colors, no material colors: Basic VC
same(S.gameLook(manifest, 1, { ...colored, shaderTag: 30 }), { lit: true, texture: true, vertexColors: true, material: false }, 'Basic VC');
// Vertex colors are read even when NiVertexColorProperty ignores them, but only if the mesh has some
same(S.gameLook(manifest, 0, { ...colored, vertexColors: 0 }).vertexColors, true, 'shader reads vertex colors');
same(S.gameLook(manifest, 0, {}).vertexColors, false, 'mesh without vertex colors');
same(S.gameLook(manifest, 3, colored), { lit: false, texture: false, vertexColors: true, material: false }, 'Basic NL VC NT');
// Fixed function: NiVertexColorProperty and the material decide
same(S.gameLook(manifest, 2, { ...colored, vertexColors: 0 }), { lit: true, texture: true, vertexColors: false, material: true }, 'fixed function');
// Without the zone's lighting the viewer lights scenery itself
same(S.gameLook({ ...manifest, lighting: null }, 0, colored), null, 'no lighting');

if (failures) {
	console.error(`${failures} failed`);
	process.exit(1);
}
console.log('scenery-core: all passed');
