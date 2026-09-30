#pragma once

#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

/**
 * LU Toolbox's color palette (its materials/__init__.py), which it colors imported models with instead of the brick
 * database's Materials.xml: LU's colors in linear RGB, the LDD colors LU doesn't have mapped onto the nearest LU one,
 * which colors are transparent, glow and metallic, how much each color varies from brick to brick, and the icon
 * renderer's corrections. Also the brick to brick color variation itself (process_model.py apply_color_variation).
 * Pure.
 */
namespace UgcPalette {
	// A material id's color in linear RGB; nullopt when LU Toolbox doesn't know it (it uses black, 26, then).
	// `icon`: with the icon renderer's corrections (white and black are toned down).
	std::optional<glm::vec3> Linear(uint32_t id, bool icon = false);

	bool IsTransparent(uint32_t id);

	// The glow color (linear) of a glowing material, nullopt for the others
	std::optional<glm::vec3> Glow(uint32_t id);

	// How much of the color variation a color gets (CUSTOM_VARIATION: black varies less, orange more); 1 by default
	float VariationScale(uint32_t id);

	// The color LU Toolbox falls back to for unknown ids
	constexpr uint32_t FALLBACK_ID = 26;

	// sRGB <-> linear, exactly as LU Toolbox converts (color_conversions.py)
	float SrgbToLinear(float srgb);
	float LinearToSrgb(float linear);
	glm::vec3 SrgbToLinear(const glm::vec3& srgb);
	glm::vec3 LinearToSrgb(const glm::vec3& linear);

	/**
	 * LU Toolbox's color variation of one color: its HSV value, taken to a 1/2.224 gamma, shifted by
	 * `random` (0..1, mapped to -variation/200 .. +variation/200, variation in percent), clamped to 0..1 and taken back.
	 * Hue and saturation stay. `color` is linear RGB.
	 */
	glm::vec3 ApplyVariation(const glm::vec3& color, float variationPercent, float random);

	/**
	 * The random number (0..1) of one brick's material: the same for the same seed, brick and material every time,
	 * so making a model again gives the same colors, and the same in every LOD (LU Toolbox restarts its random sequence
	 * for each LOD). `brick` is the brick's index in the LXFML.
	 */
	float BrickRandom(uint64_t seed, uint32_t brick, uint32_t material);
}
