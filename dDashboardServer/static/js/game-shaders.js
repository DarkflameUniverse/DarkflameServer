/**
 * The game client's shaders for the 3D views (ES module, three.js): one ShaderMaterial program per technique family
 * (scenery-core.js gameLook says which a mesh uses, from the server's table NifFile::TechniqueFor), with the maths of
 * the client's techniques in GLSL. Everything is worked out in the game's color space (sRGB values, as Direct3D 9
 * did) and made linear at the end for three.js.
 *
 * Families: 'lego' (LEGOPPLighting: hemisphere lit sun, ambient, fresnel rim, specular and a faint reflection of the
 * default environment cube), 'basic' (BasicShaders and AlphaAsAlpha: sun * N.L + ambient per vertex, or unlit),
 * 'fixed' (fixed function: lit like basic with the material's colors), 'metal' (Metallic.fx polished metal and
 * brushed steel), 'clearPlastic', 'ocean' (the Distortion shaders' warped texture layers), 'flatSurf', 'brickWater',
 * 'darkling', 'terrain' (terrain meshes) and 'flair' (Flair.fx); the sky (Skydome.fx) is 'basic', unlit, with its
 * texture moving. The shared uniforms (lights, fog, time, environment cubes) are updated in place, so every material
 * follows the zone's lighting and its blends between scenes. Materials with the same defines share one program.
 */
import * as THREE from 'three';
import { TECHNIQUE, parseDdsCube } from '/js/scenery-core.js';

const VERTEX = /* glsl */ `
uniform vec3 gameLightColor;
uniform vec3 gameAmbient;
uniform vec3 gameLightVec;
uniform vec3 gameUpperHemi;
uniform vec3 gameSpecular;
uniform float gameTime;
uniform vec2 uvScroll;
uniform vec3 materialDiffuse;
uniform float materialAlpha;
#ifdef USE_DARK
attribute vec2 uvDark;
varying vec2 vUvDark;
#endif
varying vec2 vUv;
varying vec4 vColor;
varying vec3 vLight;       // the lit color the technique's vertex shader outputs (clamped as a COLOR output is)
varying float vLdn;        // N.L, clamped
varying vec3 vNormal;      // world space
varying vec3 vWorld;
varying vec3 vObject;      // object space position (brushed steel's noise, shiny glint's height)

vec3 hemi( vec3 n ) {
	// CalculateHemiLightInfluence: the upper hemisphere light over the lower one (the client leaves that white)
	float up = n.y + 1.0;
	return ( gameUpperHemi * up + vec3( 1.0 ) * ( 2.0 - up ) ) * 0.5;
}

void main() {
	mat4 world = modelMatrix;
#ifdef USE_INSTANCING
	world = modelMatrix * instanceMatrix;
#endif
	vec4 worldPosition = world * vec4( position, 1.0 );
	vWorld = worldPosition.xyz;
	vObject = position;
	vec3 n = normalize( mat3( world ) * normal );
	vNormal = n;
	vUv = uv;
#ifdef UV_ANIM
	vUv += uvScroll * gameTime;
#endif
#ifdef USE_DARK
	vUvDark = uvDark;
#endif
	// Vertex colors come as the file has them (sRGB bytes), which is what the game's shaders get
	vec4 vcol = vec4( 1.0 );
#if defined( USE_COLOR_ALPHA )
	vcol = color;
#elif defined( USE_COLOR )
	vcol = vec4( color, 1.0 );
#endif
#ifdef MATERIAL_COLOR
	// Fixed function and the "Material" techniques: NiMaterialProperty's color (times the vertex colors it reads)
	vcol *= vec4( materialDiffuse, materialAlpha );
#endif
#ifdef USE_INSTANCING_COLOR
	vcol.rgb *= instanceColor;
#endif
	vColor = vcol;
	float ldn = dot( gameLightVec, n );
	vLdn = clamp( ldn, 0.0, 1.0 );
#if defined( FAMILY_LEGO ) || defined( FAMILY_BRICKWATER ) || defined( FAMILY_DARKLING_SPECULAR )
	#ifdef NO_AMBIENT
	vec3 ambient = vec3( 1.0 );
	#else
	vec3 ambient = gameAmbient;
	#endif
	vec3 light = max( 0.0, ldn ) * hemi( n ) * gameLightColor + ambient;
	#ifdef LIGHT_TIMES_COLOR
	light *= vcol.rgb;
	#endif
	vLight = clamp( light, 0.0, 1.0 );
#elif defined( FAMILY_METAL )
	// g_metalDiffuse * N.L^4 + g_metalAmbient
	vLight = clamp( vec3( 0.7 ) * pow( vLdn, 4.0 ) + vec3( 0.3 ), 0.0, 1.0 );
#elif defined( FAMILY_FLAIR )
	vLight = clamp( 0.85 * gameLightColor + gameAmbient, 0.0, 1.0 );
#elif defined( UNLIT )
	vLight = vec3( 1.0 );
#else
	vLight = clamp( max( vec3( 0.0 ), gameLightColor * ldn ) + gameAmbient, 0.0, 1.0 );
#endif
	gl_Position = projectionMatrix * viewMatrix * worldPosition;
}
`;

const FRAGMENT = /* glsl */ `
uniform vec3 gameLightColor;
uniform vec3 gameAmbient;
uniform vec3 gameLightVec;
uniform vec3 gameSpecular;
uniform vec3 gameFogColor;
uniform float gameFogNear;
uniform float gameFogFar;
uniform float gameFogOn;
uniform float gameTime;
uniform vec3 materialEmissive;
uniform float materialAlpha;
uniform vec2 layerWeights;
uniform float alphaCutoff;
uniform float opacity;
uniform sampler2D map;
uniform sampler2D darkMap;
uniform sampler2D noiseMap;
uniform samplerCube envMap;
uniform vec3 glowColor;
varying vec2 vUv;
#ifdef USE_DARK
varying vec2 vUvDark;
#endif
varying vec4 vColor;
varying vec3 vLight;
varying float vLdn;
varying vec3 vNormal;
varying vec3 vWorld;
varying vec3 vObject;

// Texels come in linear (sRGB textures are decoded by the GPU); the game's maths is on sRGB values
vec3 toGame( vec3 c ) { return mix( c * 12.92, 1.055 * pow( c, vec3( 1.0 / 2.4 ) ) - 0.055, step( vec3( 0.0031308 ), c ) ); }
vec3 toLinear( vec3 c ) { return mix( c / 12.92, pow( ( c + 0.055 ) / 1.055, vec3( 2.4 ) ), step( vec3( 0.04045 ), c ) ); }
vec4 tex( sampler2D s, vec2 uv ) { vec4 t = texture2D( s, uv ); return vec4( toGame( t.rgb ), t.a ); }
vec4 cube( vec3 dir ) { vec4 t = textureCube( envMap, dir ); return vec4( toGame( t.rgb ), t.a ); }

float fresnel( float vdn ) { return ( 0.1 + 0.9 * pow( 1.0 - min( 1.0, abs( vdn ) ), 3.5 ) ) * 0.8; }

void main() {
	vec3 n = normalize( vNormal );
	if ( !gl_FrontFacing ) n = -n;
	vec3 view = normalize( cameraPosition - vWorld );
	float vdn = dot( view, n );
	vec4 vc = vColor;
	vec4 result;

#if defined( FAMILY_LEGO ) || defined( FAMILY_DARKLING_SPECULAR )
	// LEGOPP_PixelCommon4: the base color by vertex color and texture as the technique's variant takes them
	vec4 base = vec4( 1.0 );
	#if defined( USE_MAP ) && defined( HAS_COLOR )
	vec4 t = tex( map, vUv );
		#ifdef MAP_TIMES_COLOR
	base = vec4( t.rgb * vc.rgb, 1.0 );
		#else
	base = vec4( mix( vc.rgb, t.rgb, t.a ), vc.a );
		#endif
	#elif defined( USE_MAP )
	base = vec4( tex( map, vUv ).rgb, 1.0 );
	#elif defined( HAS_COLOR )
	base = vc;
	#endif
	vec3 reflected = reflect( view, n );
	vec3 refl = vdn * cube( vec3( reflected.x, -reflected.y, reflected.z ) ).rgb * 0.05;
	float fres = fresnel( vdn );
	vec3 spec = vLdn * pow( max( 0.0, dot( normalize( view + gameLightVec ), n ) ), 320.0 ) * gameSpecular;
	#ifdef LIGHT_TIMES_COLOR
	result = vec4( refl + spec + vLight + fres * base.rgb, base.a );
	#else
	result = vec4( refl + spec + ( vLight + fres ) * base.rgb, base.a );
	#endif
	#ifdef GRAYSCALE
	float gray = 0.7 * ( 0.3 * result.r + 0.7 * result.g + 0.1 * result.b + 0.2 );
	result.rgb = vec3( gray );
	#endif
	#ifdef GLOW
		#ifdef IGNORE_VERTEX_ALPHA
	result.a = 1.0;
		#endif
	result.rgb = glowColor * result.a;
	#endif
	#ifdef EMISSIVE
	// The lit color goes to the base color by the vertex alpha times the material's emissive red (animated)
		#if defined( HAS_COLOR )
	float glow = vc.a * materialEmissive.r;
		#else
	float glow = materialEmissive.r;
		#endif
		#ifdef SUPER_EMISSIVE
	result.rgb = mix( result.rgb, base.rgb * 10.0, glow );
		#else
	result.rgb = mix( result.rgb, base.rgb, glow );
		#endif
		#if defined( USE_MAP )
	result.a = tex( map, vUv ).a;
		#else
	result.a = 1.0;
		#endif
	#endif
	#ifdef SHINY_GLINT
	// A glint sweeping up the object (the game moves attr_shinyGlintHeight itself)
	float sweep = fract( gameTime * 0.25 ) * 12.0 - 2.0;
	result.rgb += vec3( pow( clamp( 1.0 - abs( sweep - vObject.y ), 0.0, 1.0 ), 4.0 ) );
	#endif
	#ifdef FAMILY_DARKLING_SPECULAR
	vec4 dark = tex( darkMap, vUvDark );
	float centre = abs( materialEmissive.r - vc.a );
	float window = 1.0 - clamp( ( min( centre, 1.0 - centre ) - materialEmissive.g * 0.5 ) * materialEmissive.b * 100.0, 0.0, 1.0 );
	result = vec4( mix( result.rgb, dark.rgb, dark.a * window ), 1.0 );
	#endif

#elif defined( FAMILY_DARKLING )
	vec4 t = tex( map, vUv );
	vec4 dark = tex( darkMap, vUvDark );
	#ifdef NON_DECAL
	vec3 combined = t.rgb * vc.rgb * vLight;
	result = vec4( mix( combined, dark.rgb, dark.a * vc.a ), 1.0 );
	#else
	float centre = abs( materialEmissive.r - vc.a );
	float window = 1.0 - clamp( ( min( centre, 1.0 - centre ) - materialEmissive.g * 0.5 ) * materialEmissive.b * 100.0, 0.0, 1.0 );
	vec3 combined = mix( vc.rgb, t.rgb, t.a ) * vLight;
	result = vec4( mix( combined, dark.rgb, dark.a * window ), 1.0 );
	#endif

#elif defined( FAMILY_METAL )
	// Lighting_PolishedMetal / Lighting_BrushedSteel: the lit tint plus the metal cube's reflection
	vec4 tint = vc;
	#ifdef USE_MAP
	vec4 t = tex( map, vUv );
		#ifdef HAS_COLOR
	tint = vec4( vc.rgb * t.rgb, vc.a * t.a );
		#else
	tint = t;
		#endif
	#endif
	vec3 reflected = reflect( view, n );
	vec4 envColor = cube( reflected );
	float reflIntensity = 1.0 - ( vLdn / 2.0 + 0.5 );
	#ifdef BRUSHED
	vec3 p = normalize( vObject );
	vec4 noise = tex( noiseMap, vec2( p.x * ( p.x - p.z ), vObject.y / 5.0 ) );
	float specIntensity = pow( max( 0.0, dot( normalize( view + gameLightVec ), n ) ), 100.0 * noise.a ) * 0.5;
	vec3 coloured = envColor.rgb * noise.rgb * ( envColor.a - reflIntensity ) * tint.rgb * 2.0;
	#else
	float specIntensity = pow( max( 0.0, dot( normalize( view + gameLightVec ), n ) ), 50.0 );
	vec3 coloured = envColor.rgb * ( envColor.a - reflIntensity ) * tint.rgb * 2.0;
	#endif
	result = vec4( vLight * tint.rgb + coloured + gameSpecular * specIntensity * vLdn, tint.a );

#elif defined( FAMILY_CLEAR_PLASTIC )
	// ClearPlastic_PS: the reflection, a fresnel rim of the lit grey and a tight highlight; see-through facing the eye
	float avdn = abs( vdn );
	vec3 reflected = reflect( view, n );
	vec3 refl = cube( vec3( reflected.x, -reflected.y, reflected.z ) ).rgb;
	float ldn = dot( gameLightVec, n );
	vec3 diffuse = clamp( vec3( max( 0.0, ldn ) ) + vec3( 0.7, 0.71, 0.75 ), 0.0, 1.0 );
	float hdn = pow( max( 0.0, dot( normalize( view + gameLightVec ), n ) ), 120.0 ) * 4.19;
	float fres = ( 0.1 + 0.9 * pow( 1.0 - avdn, 3.5 ) ) * 0.5;
	result = clamp( vec4( refl + fres * diffuse * 2.5 + max( 0.0, ldn * hdn ), ( 1.0 - avdn ) * 0.5 ), 0.0, 1.0 );

#elif defined( FAMILY_OCEAN )
	// Ocean_Distort: texture layers at 1/2, 3/4 and 1 scale, each warped by the one before, moving on their own
	vec2 uv = vUv;
	vec4 layer1 = tex( map, uv * 0.5 + vec2( gameTime * 0.011, gameTime * 0.004 ) );
	vec2 warped = uv * 0.75 + vec2( -gameTime * 0.007, gameTime * 0.009 ) + layer1.rg * 0.2 - 0.5;
	vec4 layer2 = tex( map, warped );
	warped = uv + vec2( gameTime * 0.005, -gameTime * 0.006 ) + layer2.rg * 0.2 - 0.5;
	vec4 layer3 = tex( map, warped );
	vec4 water = ( layer1 + layer2 + layer3 ) * 0.3333;
	#ifdef OCEAN_FX
	float centre = abs( materialEmissive.r - water.a );
	water.a *= 1.0 - clamp( ( min( centre, 1.0 - centre ) - materialEmissive.g * 0.5 ) * materialEmissive.b * 100.0, 0.0, 1.0 );
	vec4 light = vc;
	#elif defined( UNLIT )
	vec4 light = vec4( vc.rgb * 2.0, vc.a );
	#else
	vec4 light = vec4( vLight * vc.rgb, vc.a );
	#endif
	result = water * light;

#elif defined( FAMILY_FLAT_SURF )
	// Ocean_FlatSurf: the texture lit, its alpha from a second (differently moving) lookup
	vec4 t = tex( map, vUv );
	result = vec4( t.rgb * vLight * vc.rgb, tex( map, vUv * 0.5 + vec2( gameTime * 0.01 ) ).a );

#elif defined( FAMILY_BRICKWATER )
	// BrickWater: hemisphere lit, fresnel and specular on the vertex color (the parallax normal map left out)
	float fres = fresnel( vdn );
	vec3 spec = vLdn * pow( max( 0.0, dot( normalize( view + gameLightVec ), n ) ), 320.0 ) * gameSpecular;
	result = vec4( ( vLight + fres ) * vc.rgb + spec, 1.0 );

#elif defined( FAMILY_TERRAIN )
	vec4 t = vec4( 1.0 );
	#ifdef USE_MAP
	t = tex( map, vUv );
	#endif
	#ifdef RIM_LIGHT
	float rim = clamp( 2.0 * pow( 1.0 - clamp( 2.0 * vdn, 0.0, 1.0 ), 2.0 ), 0.0, 1.0 );
	result = vec4( t.rgb * vc.rgb * ( vLight + rim ), 1.0 );
	#elif defined( DIFFUSE_ONLY )
	result = vec4( t.rgb * 2.0 * vLight, 1.0 );
	#else
	result = vec4( t.rgb * vc.rgb * vLight, 1.0 );
	#endif

#else
	// Basic, AlphaAsAlpha, fixed function, flairs, the sky: texture times the lit (or unlit) vertex color
	vec4 light = vec4( vLight * vc.rgb, vc.a );
	#ifdef LAYERS_BLENDED
	result = vec4( mix( tex( darkMap, vUvDark ).rgb, tex( map, vUv ).rgb, vc.a ) * light.rgb, 1.0 );
	#elif defined( LAYERS_ADDED )
	result = ( tex( map, vUv ) * layerWeights.x + tex( darkMap, vUvDark ) * layerWeights.y ) * light;
	#elif defined( BASIC_EMISSIVE )
	vec4 t = tex( map, vUv );
	result = vec4( vc.rgb * t.rgb * t.a + t.rgb * vc.rgb * ( gameLightColor * vLdn + gameAmbient ), vc.a * t.a );
	#elif defined( USE_MAP )
	vec4 t = tex( map, vUv );
		#if defined( DECAL ) && defined( HAS_COLOR )
	result = vec4( mix( vc.rgb, t.rgb, t.a ) * vLight, vc.a );
		#elif defined( DECAL ) || defined( IGNORE_TEXTURE_ALPHA )
	result = vec4( t.rgb, 1.0 ) * light;
		#else
	result = t * light;
		#endif
	#else
	result = light;
	#endif
	#ifdef ANIM_ALPHA
	result.a *= materialAlpha;
	#endif
#endif

	result = clamp( result, 0.0, 1.0 );
	result.a *= opacity;
	if ( result.a < alphaCutoff ) discard;
#ifdef USE_GAME_FOG
	float fogAmount = gameFogOn * clamp( ( length( vWorld - cameraPosition ) - gameFogNear ) / max( 1.0, gameFogFar - gameFogNear ), 0.0, 1.0 );
	result.rgb = mix( result.rgb, gameFogColor, fogAmount );
#endif
	gl_FragColor = vec4( toLinear( result.rgb ), result.a );
	#include <colorspace_fragment>
}
`;

// A 1x1 cube of one color, until the client's environment cube is loaded
function plainCube(value) {
	const faces = [];
	for (let i = 0; i < 6; i++) {
		const face = new THREE.DataTexture(new Uint8Array([value, value, value, 255]), 1, 1, THREE.RGBAFormat);
		face.needsUpdate = true;
		faces.push(face);
	}
	const cube = new THREE.CubeTexture(faces);
	cube.colorSpace = THREE.SRGBColorSpace;
	cube.userData.shared = true;
	cube.needsUpdate = true;
	return cube;
}

const WHITE = new THREE.DataTexture(new Uint8Array([255, 255, 255, 255]), 1, 1, THREE.RGBAFormat);
WHITE.userData.shared = true;
WHITE.needsUpdate = true;

/**
 * The shared state of the game's shaders: the zone's lights (setLights, blendTo), fog, time and the client's
 * environment cubes, fetched from envUrl(name) ('reflection', 'polished', 'brushed', 'brushedNoise') when a material
 * first needs them. material(look, mesh, maps) makes a mesh's material.
 */
export function createGameShading({ envUrl } = {}) {
	const uniforms = {
		gameLightColor: { value: new THREE.Vector3(1, 1, 1) }, gameAmbient: { value: new THREE.Vector3(0.4, 0.4, 0.4) },
		gameLightVec: { value: new THREE.Vector3(0, 1, 0) }, gameUpperHemi: { value: new THREE.Vector3(1, 1, 1) },
		gameSpecular: { value: new THREE.Vector3(0.3, 0.3, 0.3) },
		gameLightOn: { value: 0 }, // 1 once a manifest brought the zone's lighting (the terrain uses its own light until then)
		gameFogColor: { value: new THREE.Vector3(1, 1, 1) }, gameFogNear: { value: 0 }, gameFogFar: { value: 0 }, gameFogOn: { value: 0 },
		gameTime: { value: 0 }
	};
	const envMaps = {
		reflection: { value: plainCube(128), loading: null },
		polished: { value: plainCube(160), loading: null },
		brushed: { value: plainCube(140), loading: null },
		brushedNoise: { value: WHITE, loading: null }
	};

	function loadEnv(name) {
		const entry = envMaps[name];
		if (entry.loading || !envUrl) return entry;
		entry.loading = fetch(envUrl(name), { credentials: 'same-origin' }).then((r) => (r.ok ? r.arrayBuffer() : null)).then((buffer) => {
			if (!buffer) return;
			const cube = parseDdsCube(buffer, 256);
			if (cube) {
				const faces = cube.faces.map((face) => {
					const texture = new THREE.DataTexture(face.data, face.width, face.height, THREE.RGBAFormat);
					texture.needsUpdate = true;
					return texture;
				});
				const texture = new THREE.CubeTexture(faces);
				texture.colorSpace = THREE.SRGBColorSpace;
				texture.generateMipmaps = true;
				texture.minFilter = THREE.LinearMipmapLinearFilter;
				texture.userData.shared = true;
				texture.needsUpdate = true;
				entry.value = texture;
			} else {
				// A plain DDS (the brushed noise)
				const face = parseDdsCube(buffer, 256, true);
				if (!face) return;
				const texture = new THREE.DataTexture(face.data, face.width, face.height, THREE.RGBAFormat);
				texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
				texture.colorSpace = THREE.SRGBColorSpace;
				texture.generateMipmaps = true;
				texture.minFilter = THREE.LinearMipmapLinearFilter;
				texture.userData.shared = true;
				texture.needsUpdate = true;
				entry.value = texture;
			}
			for (const uniform of entry.users) uniform.value = entry.value;
		}).catch(() => {});
		return entry;
	}
	for (const entry of Object.values(envMaps)) entry.users = new Set();
	// A uniform following an environment texture (swapped in place once it's loaded)
	function envUniform(name) {
		const entry = loadEnv(name);
		const uniform = { value: entry.value };
		entry.users.add(uniform);
		return uniform;
	}

	/**
	 * The material for a mesh drawn with `look` (gameLook): maps {map, darkMap} (three.js textures or null), options
	 * {transparent, depthWrite, blending, side, alphaCutoff, opacity}.
	 */
	function material(look, mesh, maps, options) {
		const defines = {};
		// A darkling mesh without its dark texture is drawn as the LEGO shader it builds on
		const family = look.family === 'darkling' && !maps.darkMap ? 'lego' : look.family;
		const define = (name, on = true) => { if (on) defines[name] = ''; };
		define('FAMILY_' + ({ lego: 'LEGO', metal: 'METAL', clearPlastic: 'CLEAR_PLASTIC', ocean: 'OCEAN', flatSurf: 'FLAT_SURF', brickWater: 'BRICKWATER',
			darkling: look.flags & TECHNIQUE.SPECULAR ? 'DARKLING_SPECULAR' : 'DARKLING', terrain: 'TERRAIN', flair: 'FLAIR' }[family] || 'BASIC'));
		const hasColor = !!look.vertexColors;
		define('HAS_COLOR', hasColor);
		define('USE_MAP', !!maps.map);
		define('USE_DARK', !!maps.darkMap);
		define('UNLIT', !look.lit);
		// Metal without vertex colors takes the material's (Lighting_PolishedMetal_VS)
		define('MATERIAL_COLOR', !!look.material || (family === 'metal' && !hasColor));
		define('UV_ANIM', !!look.uvAnim && !!(mesh.uvScroll && (mesh.uvScroll[0] || mesh.uvScroll[1])));
		define('NO_AMBIENT', !!(look.flags & TECHNIQUE.NO_AMBIENT));
		// The LEGO "VertColor" variants multiply the light by the vertex color in the vertex shader
		define('LIGHT_TIMES_COLOR', family === 'lego' && hasColor && !maps.map);
		define('MAP_TIMES_COLOR', (!!look.emissive || !!(look.flags & TECHNIQUE.NON_DECAL)) && hasColor);
		define('GRAYSCALE', !!(look.flags & TECHNIQUE.GRAYSCALE));
		define('GLOW', !!(look.flags & TECHNIQUE.GLOW));
		define('IGNORE_VERTEX_ALPHA', !!(look.flags & TECHNIQUE.IGNORE_VERTEX_ALPHA));
		define('EMISSIVE', !!look.emissive);
		define('SUPER_EMISSIVE', !!(look.flags & TECHNIQUE.SUPER_EMISSIVE));
		define('SHINY_GLINT', !!(look.flags & TECHNIQUE.SHINY_GLINT));
		define('NON_DECAL', !!(look.flags & TECHNIQUE.NON_DECAL));
		define('BRUSHED', look.metal === 'brushed');
		define('OCEAN_FX', !!(look.flags & TECHNIQUE.OCEAN_FX));
		define('RIM_LIGHT', !!(look.flags & TECHNIQUE.RIM_LIGHT));
		define('DIFFUSE_ONLY', !!(look.flags & TECHNIQUE.DIFFUSE_ONLY));
		define('ANIM_ALPHA', !!(look.flags & TECHNIQUE.ANIM_ALPHA));
		define('BASIC_EMISSIVE', !!(look.flags & TECHNIQUE.BASIC_EMISSIVE));
		define('LAYERS_BLENDED', !!maps.darkMap && look.layers === 'blended');
		define('LAYERS_ADDED', !!maps.darkMap && look.layers === 'added');
		define('DECAL', look.textureAlpha === 'decal');
		define('IGNORE_TEXTURE_ALPHA', look.textureAlpha === 'ignored');
		define('USE_GAME_FOG', !(look.flags & TECHNIQUE.NO_FOG) && !look.sky);
		const env = family === 'metal' ? envUniform(look.metal === 'brushed' ? 'brushed' : 'polished')
			: family === 'lego' || family === 'clearPlastic' || (family === 'darkling' && look.flags & TECHNIQUE.SPECULAR) ? envUniform('reflection') : { value: envMaps.reflection.value };
		const diffuse = mesh.diffuse || [1, 1, 1];
		const emissive = mesh.emissive || [0, 0, 0];
		const shaderMaterial = new THREE.ShaderMaterial({
			vertexShader: VERTEX,
			fragmentShader: FRAGMENT,
			defines,
			uniforms: {
				...uniforms,
				map: { value: maps.map || WHITE }, darkMap: { value: maps.darkMap || WHITE },
				noiseMap: family === 'metal' && look.metal === 'brushed' ? envUniform('brushedNoise') : { value: WHITE },
				envMap: env,
				uvScroll: { value: new THREE.Vector2(...(mesh.uvScroll || [0, 0])) },
				materialDiffuse: { value: new THREE.Vector3(...diffuse) }, materialAlpha: { value: mesh.alpha ?? 1 },
				materialEmissive: { value: new THREE.Vector3(...emissive) },
				layerWeights: { value: new THREE.Vector2(diffuse[0], diffuse[1]) },
				glowColor: { value: new THREE.Vector3(0, 1, 1) },
				alphaCutoff: { value: options.alphaCutoff || 0 },
				opacity: { value: options.opacity ?? 1 }
			},
			vertexColors: hasColor,
			transparent: !!options.transparent,
			depthWrite: options.depthWrite !== false,
			blending: options.blending ?? THREE.NormalBlending,
			side: options.side ?? THREE.FrontSide
		});
		shaderMaterial.toneMapped = false;
		shaderMaterial.userData.game = true;
		return shaderMaterial;
	}

	return {
		uniforms,
		material,
		/** The zone's lighting (manifest.lighting) at once. */
		setLights(lighting) {
			uniforms.gameLightOn.value = lighting ? 1 : 0;
			if (!lighting) return;
			uniforms.gameLightColor.value.fromArray(lighting.light);
			uniforms.gameAmbient.value.fromArray(lighting.ambient);
			uniforms.gameLightVec.value.fromArray(lighting.lightVec);
			uniforms.gameUpperHemi.value.fromArray(lighting.upperHemi || [1, 1, 1]);
			uniforms.gameSpecular.value.fromArray(lighting.specular || [0.3, 0.3, 0.3]);
			uniforms.gameFogColor.value.fromArray(lighting.fogColor || [1, 1, 1]);
			uniforms.gameFogNear.value = lighting.fogNear || 0;
			uniforms.gameFogFar.value = lighting.fogFar || 0;
		},
		/** Fog as the zone's lighting has it (off by default: the views look from much further out than the game). */
		setFog(on) { uniforms.gameFogOn.value = on ? 1 : 0; },
		/** Seconds since the view started, for the moving textures. */
		tick(dt) { uniforms.gameTime.value += dt; }
	};
}
