#include "UgcPalette.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {
	using Table = std::unordered_map<uint32_t, glm::vec3>;

	glm::vec3 FromSrgb(float r, float g, float b) {
		return UgcPalette::SrgbToLinear(glm::vec3(r, g, b));
	}

	// Colors are linear, as LU Toolbox keeps them
	const Table& Opaque() {
		static const Table table = [] {
			Table t = {
				{ 1, { 0.904661f, 0.904661f, 0.904661f } },
				{ 5, { 0.693872f, 0.491021f, 0.194618f } },
				{ 18, { 0.672443f, 0.168269f, 0.051269f } },
				{ 21, { 0.730461f, 0.0f, 0.004025f } },
				{ 23, { 0.0f, 0.095307f, 0.391572f } },
				{ 24, { 0.991102f, 0.552011f, 0.0f } },
				{ 26, { 0.006f, 0.006f, 0.006f } },
				{ 28, { 0.0f, 0.198069f, 0.021219f } },
				{ 37, { 0.0f, 0.304987f, 0.017642f } },
				{ 38, { 0.391572f, 0.046665f, 0.007499f } },
				{ 50, { 0.7399f, 0.7399f, 0.7399f } },
				{ 102, { 0.06301f, 0.262251f, 0.564712f } },
				{ 106, { 0.799103f, 0.124772f, 0.009134f } },
				{ 107, { 0.002732f, 0.462077f, 0.462077f } },
				{ 119, { 0.296138f, 0.47932f, 0.003035f } },
				{ 120, { 0.672444f, 0.768151f, 0.262251f } },
				{ 124, { 0.332452f, 0.0f, 0.147027f } },
				{ 135, { 0.111932f, 0.174647f, 0.262251f } },
				{ 138, { 0.262251f, 0.177888f, 0.084376f } },
				{ 140, { 0.0f, 0.0185f, 0.052861f } },
				{ 141, { 0.0f, 0.03434f, 0.008023f } },
				{ 151, { 0.114435f, 0.223228f, 0.130137f } },
				{ 154, { 0.215861f, 0.002428f, 0.01096f } },
				{ 191, { 0.887923f, 0.318547f, 0.0f } },
				{ 192, { 0.104617f, 0.011612f, 0.003677f } },
				{ 194, { 0.332452f, 0.283149f, 0.283149f } },
				{ 199, { 0.072272f, 0.082283f, 0.093059f } },
				{ 208, { 0.768151f, 0.768151f, 0.693872f } },
				{ 212, { 0.242281f, 0.520996f, 0.83077f } },
				{ 221, { 0.730461f, 0.038204f, 0.258183f } },
				{ 222, { 0.846873f, 0.341914f, 0.53948f } },
				{ 226, { 1.0f, 0.768151f, 0.141263f } },
				{ 268, { 0.066626f, 0.011612f, 0.341914f } },
				{ 283, { 0.913099f, 0.533276f, 0.250158f } },
				{ 294, { 0.991102f, 0.973445f, 0.665388f } },
				{ 308, { 0.03434f, 0.015209f, 0.0f } },
				{ 329, { 1.0f, 1.0f, 1.0f } },
				{ 330, { 0.184475f, 0.184475f, 0.076185f } },
				{ 9013, { 1.0f, 0.009721f, 0.020289f } },
				{ 9014, { 0.799103f, 0.088655f, 0.0f } },
				{ 9015, { 1.0f, 0.630757f, 0.033105f } },
				{ 9016, { 0.012983f, 1.0f, 0.090842f } },
				{ 9017, { 0.059511f, 0.768152f, 0.913099f } },
				{ 9018, { 0.0f, 0.226966f, 1.0f } },
				{ 9019, { 0.40724f, 0.012983f, 1.0f } },
				{ 9020, { 0.686686f, 0.000303f, 1.0f } },
			};
			// LDD colors LU doesn't have, as the nearest LU color
			const std::pair<uint32_t, uint32_t> aliases[] = {
				{ 0, 26 }, { 2, 194 }, { 3, 5 }, { 4, 106 }, { 6, 119 }, { 9, 222 }, { 11, 212 }, { 12, 106 }, { 13, 191 }, { 19, 191 },
				{ 22, 221 }, { 25, 192 }, { 27, 199 }, { 29, 151 }, { 36, 283 }, { 39, 194 }, { 45, 212 }, { 100, 283 }, { 101, 106 },
				{ 103, 194 }, { 104, 268 }, { 105, 191 }, { 110, 23 }, { 112, 23 }, { 115, 119 }, { 116, 107 }, { 118, 212 }, { 326, 120 },
				{ 125, 283 }, { 128, 38 }, { 133, 106 }, { 134, 119 }, { 136, 135 }, { 153, 138 }, { 180, 191 }, { 195, 23 }, { 196, 23 },
				{ 198, 124 }, { 216, 154 }, { 217, 138 }, { 218, 124 }, { 219, 268 }, { 223, 222 }, { 232, 212 }, { 233, 37 }, { 295, 222 },
				{ 312, 138 }, { 321, 102 }, { 322, 212 }, { 323, 208 }, { 324, 124 }, { 325, 222 },
			};
			for (const auto& [id, target] : aliases) t[id] = t.at(target);
			return t;
		}();
		return table;
	}

	const Table& Transparent() {
		static const Table table = [] {
			Table t = {
				{ 20, { 0.930111f, 0.672443f, 0.250158f } },
				{ 40, { 0.854993f, 0.854993f, 0.854993f } },
				{ 41, FromSrgb(0.674509f, 0.0f, 0.0f) },
				{ 42, FromSrgb(0.244106f, 0.720966f, 0.772058f) },
				{ 43, FromSrgb(0.031372f, 0.285668f, 0.643137f) },
				{ 44, FromSrgb(0.858f, 0.771375f, 0.0f) },
				{ 47, FromSrgb(0.986f, 0.336526f, 0.120035f) },
				{ 48, FromSrgb(0.0f, 0.391f, 0.0f) },
				{ 49, FromSrgb(0.697f, 1.0f, 0.0f) },
				{ 111, FromSrgb(0.741177f, 0.670588f, 0.639216f) },
				{ 113, FromSrgb(0.754717f, 0.060520f, 0.541647f) },
				{ 126, FromSrgb(0.267974f, 0.196078f, 0.627451f) },
				{ 143, FromSrgb(0.325985f, 0.551358f, 0.821f) },
				{ 182, FromSrgb(0.913726f, 0.524575f, 0.0156863f) },
				{ 311, FromSrgb(0.454640f, 0.788235f, 0.0980392f) },
			};
			const std::pair<uint32_t, uint32_t> aliases[] = { { 157, 44 }, { 230, 113 }, { 231, 182 }, { 234, 44 }, { 284, 126 }, { 285, 111 }, { 293, 43 } };
			for (const auto& [id, target] : aliases) t[id] = t.at(target);
			return t;
		}();
		return table;
	}

	const Table& GlowColors() {
		static const Table table = [] {
			const auto& opaque = Opaque();
			Table t = { { 50, { 0.401978f, 0.401978f, 0.401978f } } };
			for (const uint32_t id : { 329u, 294u, 9013u, 9014u, 9015u, 9016u, 9017u, 9018u, 9019u, 9020u }) t[id] = opaque.at(id);
			const std::pair<uint32_t, uint32_t> aliases[] = {
				{ 9000, 9020 }, { 9002, 9016 }, { 9004, 9018 }, { 9008, 9013 }, { 9009, 9014 }, { 9010, 9016 }, { 9011, 9017 }, { 9012, 9019 },
				{ 9021, 329 }, { 9022, 50 }, { 9023, 9013 }, { 9024, 9014 }, { 9025, 9016 }, { 9026, 9018 }, { 9027, 329 },
			};
			for (const auto& [id, target] : aliases) t[id] = t.at(target);
			return t;
		}();
		return table;
	}

	const Table& Metallic() {
		static const Table table = [] {
			Table t;
			const auto add = [&t](std::initializer_list<uint32_t> ids, glm::vec3 color) { for (const auto id : ids) t[id] = color; };
			add({ 131, 150, 179, 298, 315 }, { 0.262251f, 0.296138f, 0.296138f });
			add({ 139, 187, 300 }, { 0.174648f, 0.066626f, 0.029557f });
			add({ 148 }, { 0.06301f, 0.051269f, 0.043735f });
			add({ 149 }, { 0.006f, 0.006f, 0.006f });
			add({ 184 }, { 0.238095f, 0.00907f, 0.00907f });
			add({ 186, 200 }, { 0.081104f, 0.252379f, 0.045668f });
			add({ 145, 185 }, { 0.104617f, 0.177888f, 0.278894f });
			add({ 309, 183 }, { 0.617207f, 0.617207f, 0.617207f });
			add({ 297, 147, 189 }, { 0.401978f, 0.212231f, 0.027321f });
			add({ 310, 127 }, { 0.737911f, 0.533276f, 0.181164f });
			return t;
		}();
		return table;
	}

	const std::unordered_map<uint32_t, float>& CustomVariation() {
		static const std::unordered_map<uint32_t, float> table = {
			{ 1, 1.3f }, { 21, 1.4f }, { 23, 1.25f }, { 24, 1.5f }, { 26, 0.4f }, { 28, 0.8f }, { 37, 0.8f }, { 135, 0.85f }, { 141, 0.7f },
			{ 199, 0.7f }, { 192, 0.75f }, { 212, 1.25f }, { 222, 1.05f }, { 226, 1.75f }, { 283, 1.15f }, { 308, 0.85f }, { 323, 1.4f }, { 326, 1.75f },
		};
		return table;
	}

	uint64_t SplitMix(uint64_t x) {
		x += 0x9E3779B97F4A7C15ull;
		x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
		x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
		return x ^ (x >> 31);
	}
}

namespace UgcPalette {
	std::optional<glm::vec3> Linear(uint32_t id, bool icon) {
		if (icon) {
			// ICON_MATERIALS_OPAQUE
			if (id == 1) return FromSrgb(0.7f, 0.7f, 0.7f);
			if (id == 26) return FromSrgb(0.01f, 0.01f, 0.01f);
		}
		// The importer loads opaque, then transparent, metallic and glow colors, the first one of an id winning
		for (const auto* table : { &Opaque(), &Transparent(), &Metallic(), &GlowColors() }) {
			if (const auto it = table->find(id); it != table->end()) return it->second;
		}
		return std::nullopt;
	}

	bool IsTransparent(uint32_t id) {
		return Transparent().contains(id);
	}

	bool IsMetallic(uint32_t id) {
		return Metallic().contains(id);
	}

	std::optional<glm::vec3> Glow(uint32_t id) {
		const auto it = GlowColors().find(id);
		return it != GlowColors().end() ? std::optional(it->second) : std::nullopt;
	}

	float VariationScale(uint32_t id) {
		const auto it = CustomVariation().find(id);
		return it != CustomVariation().end() ? it->second : 1.0f;
	}

	float SrgbToLinear(float srgb) {
		return srgb <= 0.0404482362771082f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
	}

	float LinearToSrgb(float linear) {
		return linear > 0.0031308f ? 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f : 12.92f * linear;
	}

	glm::vec3 SrgbToLinear(const glm::vec3& srgb) {
		return { SrgbToLinear(srgb.r), SrgbToLinear(srgb.g), SrgbToLinear(srgb.b) };
	}

	glm::vec3 LinearToSrgb(const glm::vec3& linear) {
		return { LinearToSrgb(linear.r), LinearToSrgb(linear.g), LinearToSrgb(linear.b) };
	}

	glm::vec3 ApplyVariation(const glm::vec3& color, float variationPercent, float random) {
		const float value = std::max({ color.r, color.g, color.b });
		float gamma = std::pow(std::max(value, 0.0f), 1.0f / 2.224f);
		const float range = variationPercent / 200.0f;
		gamma += -range + random * 2.0f * range;
		const float newValue = std::pow(std::clamp(gamma, 0.0f, 1.0f), 2.224f);
		// Setting an HSV value keeps hue and saturation: the channels scale together (a black color becomes grey)
		if (value <= 0.0f) return glm::vec3(newValue);
		return color * (newValue / value);
	}

	float BrickRandom(uint64_t seed, uint32_t brick, uint32_t material) {
		const auto bits = SplitMix(SplitMix(seed ^ (static_cast<uint64_t>(brick) << 32)) ^ material);
		return static_cast<float>(bits >> 40) / static_cast<float>(1ull << 24);
	}
}
