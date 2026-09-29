#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

/**
 * The glitter the UGC server gives glitter colors (docs/UgcServer.md, "Metal and glow"): a tileable texture of white
 * flecks (its alpha) laid over the brick's color by the client's LEGO-AnimUV shader (lerp(vertex color, texture,
 * texture alpha), then the LEGO lighting), on UVs projected from the model's own coordinates so every brick gets the
 * same density, drifting as the texture transform's translation loops. Pure.
 */
namespace UgcGlitter {
	// The texture's side in pixels (a power of two, mipmapped down to 1)
	constexpr int TEXTURE_SIZE = 128;

	struct Params {
		float tile{ 1.6f };    // glitter_size: the texture's side in model units (LDD units: a stud is 0.8)
		uint32_t flecks{ 50 }; // glitter_density: flecks in one tile
		float speed{ 1.0f };   // glitter_speed: 1 moves the flecks a tile in U in 7 s and in V in 11 s; 0 keeps them still

		// Seconds the texture's translation takes to go one tile in U and in V (0: no animation)
		float PeriodU() const { return speed > 0.0f ? 7.0f / speed : 0.0f; }
		float PeriodV() const { return speed > 0.0f ? 11.0f / speed : 0.0f; }
		bool operator==(const Params&) const = default;
	};

	// The texture's alpha (TEXTURE_SIZE squared, rows top to bottom): `flecks` soft dots at the same places every time,
	// wrapping around the edges so the texture tiles. Its color is white.
	std::vector<uint8_t> FleckAlpha(uint32_t flecks);

	// The texture's mipmaps' alpha, from TEXTURE_SIZE down to 1 (each the mean of 2x2 of the one before)
	std::vector<std::vector<uint8_t>> Mipmaps(const std::vector<uint8_t>& alpha);

	// A vertex's UV: its position on the axis plane its normal faces most, in tiles
	glm::vec2 Uv(const glm::vec3& position, const glm::vec3& normal, float tile);

	// The texture's alpha (0..1) at `uv` (wrapping, bilinear)
	float Sample(const std::vector<uint8_t>& alpha, const glm::vec2& uv);
}
