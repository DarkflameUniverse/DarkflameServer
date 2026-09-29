#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

/**
 * The glitter the UGC server gives glitter colors (docs/UgcServer.md, "Glitter"). Pure.
 *
 * Flecks: a tileable texture of white flecks (its alpha) laid over the brick's color by the client's LEGO-AnimUV shader
 * (lerp(vertex color, texture, texture alpha), then the LEGO lighting). They stay still: a placed player model is never
 * updated after it loads (LWOSkinnedRenderComponent::Run with animation off for modelType 2), so texture controllers in
 * its .nif never run.
 *
 * Sparkles: a second shape over each glitter brick drawn by the client's Distortion Directional (Ocean) shader, whose
 * texture layers the client moves every frame whatever the object does (the shader's Run sets
 * g_vDirectionalMotionLayer1..3). Its texture has flat sparkles at an alpha that one layer's alone keeps under the
 * alpha test, so a sparkle shows only where two moving layers' sparkles meet: points that flash and go out.
 *
 * Both are placed on each brick by UVs projected from the model's own coordinates, turned and moved by a number of the
 * brick's own (BrickSeed), so no two bricks have the same pattern and a model made again has the same one.
 */
namespace UgcGlitter {
	struct Params {
		// Flecks (LEGO-AnimUV)
		float tile{ 1.6f };          // glitter_size: the fleck texture's side in model units (LDD units: a stud is 0.8)
		uint32_t flecks{ 80 };       // glitter_density: flecks in one tile
		float fleckSize{ 0.05f };    // glitter_fleck_size: a fleck's diameter in model units (LDD units are cm: 0.5 mm)
		float fleckOpacity{ 80.0f }; // glitter_fleck_opacity: percent, the brightest flecks' alpha
		bool random{ true };         // glitter_random: each brick its own pattern (BrickSeed), else the same on every brick
		// Sparkles (Distortion Directional)
		float sparkleSize{ 0.1f };         // glitter_sparkle_size: a sparkle's diameter in model units
		float sparkleAmount{ 5.0f };       // glitter_sparkle_amount: percent of each moving layer covered by sparkles
		float speed{ 1.0f };               // glitter_speed: how fast sparkles flash and go out (1: about half a second)
		float sparkleTint{ 30.0f };        // glitter_sparkle_tint: percent, how far sparkles take their brick's color
		float sparkleBrightness{ 100.0f }; // glitter_sparkle_brightness: percent, the sparkles' vertex color

		// The fleck texture's side in pixels: the power of two (128 to 512) that makes a fleck at least 3 pixels wide
		int TextureSize() const;
		// The sparkle texture's side in model units. The client moves its layers a fixed share of a tile a second (a
		// tile in 24, 48 and 72 s), so the tile sets how fast they cross: 75 sparkle sizes times the speed.
		float SparkleTile() const;
		// The sparkle texture's side in pixels: the power of two (128 to 1024) that makes a sparkle 3 pixels wide
		int SparkleTextureSize() const;
		bool operator==(const Params&) const = default;
	};

	// The sparkles' alpha in their texture. The alpha test of the client's alpha test phase keeps what reaches 127
	// (ShaderCommon__SetupPhaseRenderStates: GREATEREQUAL 0x7f); the Directional shader averages 2 layers or 3 (by the
	// graphics settings), so one sparkle alone is 115 or 77 and two meeting are 230 or 153
	constexpr uint8_t SPARKLE_ALPHA = 230;
	// How far the sparkle shapes stand off their brick, along its normals (model units): in front of its surface, so
	// the brick (drawn after them when it is transparent) doesn't cover them and they don't fight it for the depth
	constexpr float SPARKLE_LIFT = 0.005f;

	// The fleck texture's alpha (TextureSize() squared, rows top to bottom): `flecks` flat flakes of about `fleckSize`
	// (0.7 to 1.3 of it) with a pixel's worth of edge, each as bright as its facet happens to catch the light (0.3 to 1
	// of `fleckOpacity`, most of them dim), at the same places every time, wrapping around the edges so the texture
	// tiles. Its color is white.
	std::vector<uint8_t> FleckAlpha(const Params& params);

	// The sparkle texture's alpha (SparkleTextureSize() squared): flat discs of sparkleSize at SPARKLE_ALPHA covering
	// sparkleAmount percent of it, at the same places every time, tiling. Its color is white.
	std::vector<uint8_t> SparkleAlpha(const Params& params);

	// A square texture's mipmaps' alpha, from its own size down to 1: each the mean of 2x2 of the one before, or for
	// the first `keepPeaks` the brightest of them (so sparkles keep their alpha at the next few distances)
	std::vector<std::vector<uint8_t>> Mipmaps(const std::vector<uint8_t>& alpha, int keepPeaks = 0);

	// A brick's number for placing its glitter (never 0), from the model's seed and the brick's index: the same for
	// the brick in every LOD and every time the model is made
	uint32_t BrickSeed(uint64_t modelSeed, uint32_t brick);

	// Which texture a UV set is for: a brick's sparkles are placed apart from its flecks
	enum class eLayer : uint8_t { FLECKS = 0, SPARKLES };

	// A vertex's UV: its position on the axis plane its normal faces most, in tiles, turned by an angle and moved by
	// an offset (under a tile) that `seed` (the brick's BrickSeed) picks for each plane and layer; seed 0 leaves it
	// as it is
	glm::vec2 Uv(const glm::vec3& position, const glm::vec3& normal, float tile, uint32_t seed = 0, eLayer layer = eLayer::FLECKS);

	// A sparkle's vertex color: white taking `sparkleTint` percent of its brick's color (sRGB), at `sparkleBrightness`,
	// alpha 1
	glm::vec4 SparkleColor(const glm::vec4& brickColor, const Params& params);

	// A square texture's alpha (0..1) at `uv` (wrapping, bilinear)
	float Sample(const std::vector<uint8_t>& alpha, const glm::vec2& uv);
}
