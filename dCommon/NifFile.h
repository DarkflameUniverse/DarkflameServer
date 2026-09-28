#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * Reads the game client's Gamebryo meshes (.nif) into flat, drawable meshes for the dashboard's 3D views, following
 * the NifTools project's format description (nif.xml). Pure (bytes in, meshes out) so it can be unit tested.
 *
 * The client's files are versions 20.2.0.8 and 20.3.0.9 with user version 0. What is drawn: the scene graph
 * (NiNode, NiLODNode, NiBillboardNode and other nodes) with its transforms baked into the vertices, NiTriShape and
 * NiTriStrips geometry (positions, normals, the first UV set, vertex colors), and the properties Gamebryo passes down
 * the tree: NiMaterialProperty, NiAlphaProperty, NiTexturingProperty's base texture (an external NiSourceTexture),
 * NiVertexColorProperty and NiStencilProperty's draw mode (double sided), and how fast NiTextureTransformControllers move
 * the base texture. Skipped: hidden subtrees, other animation,
 * particles, lights and cameras; skinned geometry is drawn in its bind pose. Blocks are skipped by their stored sizes,
 * so a block this reader doesn't know never breaks the rest of the file.
 */
namespace NifFile {
	struct Material {
		std::array<float, 3> diffuse{ 1.0f, 1.0f, 1.0f };  // sRGB 0..1
		std::array<float, 3> emissive{ 0.0f, 0.0f, 0.0f };
		float alpha{ 1.0f };
		bool alphaBlend{};
		bool alphaTest{};
		uint8_t alphaThreshold{ 128 };
		bool doubleSided{};
		// NiVertexColorProperty's source vertex mode: 0 ignore vertex colors, 1 they're emissive, 2 they're ambient and diffuse
		uint8_t vertexColorMode{ 2 };
		std::string texture;      // the base texture's file name as stored (often relative to the .nif's folder)
		int32_t embeddedTexture{ -1 }; // else the block with the texture's pixels in the file (EmbeddedTexture)
		bool clampU{};
		bool clampV{};
		// NiTexturingProperty's dark texture (its second slot), which the client's two layer shaders blend or add to
		// the base texture; as `texture` and `embeddedTexture`
		std::string darkTexture;
		int32_t embeddedDarkTexture{ -1 };
		// How fast the base texture moves (tiles a second in U and V): NiTextureTransformControllers on the
		// NiTexturingProperty translating the base map, each from its first key to its last, looping
		std::array<float, 2> uvScroll{};
		int32_t shaderTag{ -1 }; // mapShaders id from a multishader tag in the name of the mesh or a node above it
	};

	struct Mesh {
		Material material;
		std::vector<float> positions;   // x, y, z per vertex, in the model's space
		std::vector<float> normals;     // empty when the geometry has none
		std::vector<float> uvs;         // u, v per vertex, empty when none: the UV set the base texture names
		std::vector<float> uvs2;        // the UV set the dark texture names, empty without one
		std::vector<uint8_t> colors;    // r, g, b, a per vertex (sRGB), empty when none
		std::vector<uint16_t> indices;  // triangles
	};

	/**
	 * What a texture's alpha channel does, which the client's shader decides (res/shaders/*.fx), not the .nif:
	 * OPACITY it is see-through where the alpha is (Basic, AlphaAsAlpha, fixed function and most others); DECAL the
	 * texture is laid over the vertex colors by its alpha and the mesh itself stays as opaque as its vertex colors
	 * (the LEGO lighting shaders); IGNORED the alpha does nothing (LEGO items, terrain meshes).
	 */
	enum class eTextureAlpha : uint8_t { OPACITY, DECAL, IGNORED };

	// mapShaders.gameValue of the LEGO shader, the client's default
	constexpr int32_t LEGO_SHADER = 5;
	// RenderComponent's shader for models whose parts name their own shaders (mapShaders "Multishader")
	constexpr int32_t MULTISHADER = 9999;

	/**
	 * The mapShaders id in a multishader part's name: "S05__TRUNKS" or "rock_S30" (LWOBaseRenderComponent::
	 * AddObjectToRenderPipe reads "S%d", else "_S%d"); -1 for none.
	 */
	int32_t ShaderTag(std::string_view name);

	// The shader (gameValue) a multishader part is drawn with, from its tag's gameValue: outside 3..108 the LEGO shader
	int32_t MultishaderPart(std::optional<int32_t> tagShader);

	// What a texture's alpha does under a shader (mapShaders.gameValue); -1 is fixed function (opacity)
	eTextureAlpha TextureAlphaFor(int32_t shader);

	/**
	 * What a shader (mapShaders.gameValue) leaves out of the usual lit look, which is (sun * max(0, N.L) + ambient)
	 * * vertex color * texture, the scene's lights from its .lvl, NiMaterialProperty's colors unused (the client's
	 * BasicShaders.fx, LEGOPPLighting.fx and Ocean.fx). Fixed function (-1) has none of these: Gamebryo lights it with
	 * the material and NiVertexColorProperty.
	 */
	enum eShaderLook : uint16_t {
		UNLIT = 1,            // no lighting: the colors as they are (the "NoLighting" techniques)
		NO_TEXTURE = 2,       // the texture isn't sampled ("NoTexture")
		NO_VERTEX_COLORS = 4, // vertex colors aren't read
		MATERIAL_COLOR = 8,   // NiMaterialProperty's diffuse color is (the "Material" techniques)
		// Two textures (base and dark, each with its UV set): blended by the vertex alpha, which is then no opacity
		// ("Two Layers Blended"), or added, weighted by the material's diffuse red and green ("Two Textures Added")
		TWO_LAYERS_BLENDED = 16,
		TWO_LAYERS_ADDED = 32,
		// Metal (Metallic.fx): the lit color plus an environment map (textures/metal/metal_reflection_*.dds, which the
		// shader loads itself) tinted by the vertex color, polished or, with BRUSHED, brushed with object space noise
		REFLECTIVE = 64,
		BRUSHED = 128,
		// LEGO-Emissive: lerp(lit, vertex color, vertex alpha * NiMaterialProperty's emissive red); the vertex alpha is
		// that mask, not opacity
		EMISSIVE = 256,
		// Not a shader's: the UGC server's glitter groups (LEGO-AnimUV with the fleck texture it stores in the .nif,
		// UgcGlitter), white flecks by the texture's alpha over the lit vertex color, moving with the texture. Set by
		// the dashboard's UGC mesh route, not by ShaderLookFor.
		GLITTER = 512
	};

	// eShaderLook bits of a shader (mapShaders.gameValue); 0 for the usual lit look and for fixed function
	uint16_t ShaderLookFor(int32_t shader);

	// Where a node is in the model's space: p' = rotation * p + translation (row-major, scale folded in)
	struct NodeTransform {
		std::array<float, 9> rotation{ 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		std::array<float, 3> translation{};
	};

	struct Model {
		uint32_t version{};
		std::vector<Mesh> meshes;
		std::map<std::string, NodeTransform> nodes; // named nodes that are drawn (the first of each name), e.g. attach points
		std::array<float, 3> min{};
		std::array<float, 3> max{};
		std::map<std::string, uint32_t> skipped; // block types in the file that aren't drawn, with how many
		uint32_t skinned{};                       // meshes drawn in their bind pose
	};

	/**
	 * The meshes of a .nif. `lod` picks among an NiLODNode's children: 0 the most detailed (the nearest range), higher
	 * numbers coarser ones, clamped to what the node has. nullopt and `error` set when the file can't be read at all.
	 */
	std::optional<Model> Parse(std::string_view data, uint32_t lod, std::string& error);

	/**
	 * A model for the browser: a little-endian uint32 with the length of a JSON header, the header (padded with spaces
	 * to a multiple of 4), then the binary data it describes. Per mesh at "offset": float32 positions (3 per vertex),
	 * int8 normals (3 per vertex, times 127, padded to 4 bytes) when "normals", float32 UVs (2 per vertex) when "uv",
	 * float32 dark texture UVs when "uv2", uint8 RGBA colors when "colors", then uint16 indices (padded to 4 bytes).
	 * A mesh whose base texture moves has "uvScroll" (Material::uvScroll).
	 * `textures[i]` is where mesh i's texture is (empty: none) and `darkTextures[i]` its dark texture; the header
	 * lists each once in "textures" and a mesh's "texture" and "darkTexture" index it (-1: none). `looks[i]`, when
	 * given, is mesh i's "look" (eShaderLook bits of the shader it is drawn with, for views without a scenery manifest).
	 */
	std::string Encode(const Model& model, const std::vector<std::string>& textures, const std::vector<std::string>& darkTextures = {},
		const std::vector<uint16_t>& looks = {});

	/**
	 * A texture stored inside a .nif (NiPixelData or NiPersistentSrcTextureRendererData, block `block`) as a DDS file
	 * with its mipmaps: DXT1/3/5 as they are, 24 and 32-bit RGB(A) uncompressed. nullopt for other formats.
	 */
	std::optional<std::string> EmbeddedTexture(std::string_view data, int32_t block);

	// The model a Gamebryo animation set (.kfm) is for, as stored (relative to the .kfm's folder, backslashes)
	std::optional<std::string> KfmModelPath(std::string_view data);
}
