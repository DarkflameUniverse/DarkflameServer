#include "UgcGlitter.h"

#include <algorithm>
#include <cmath>

namespace UgcGlitter {
	namespace {
		uint64_t SplitMix(uint64_t x) {
			x += 0x9E3779B97F4A7C15ull;
			x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
			x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
			return x ^ (x >> 31);
		}
		// 0..1 from 24 bits of a hash
		float Unit(uint64_t bits) { return static_cast<float>(bits >> 40) / static_cast<float>(1ull << 24); }
		// The side of a square texture's alpha
		int Side(const std::vector<uint8_t>& alpha) { return static_cast<int>(std::lround(std::sqrt(static_cast<double>(alpha.size())))); }
	}

	std::vector<uint8_t> FleckAlpha(uint32_t flecks) {
		constexpr int N = TEXTURE_SIZE;
		std::vector<float> alpha(static_cast<size_t>(N) * N, 0.0f);
		// SplitMix64 from a fixed seed: the same texture on every platform
		uint64_t state = 0x6C69747465720000ull;
		const auto next = [&state] {
			uint64_t z = (state += 0x9E3779B97F4A7C15ull);
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
			return static_cast<float>((z ^ (z >> 31)) >> 40) / static_cast<float>(1ull << 24);
		};
		for (uint32_t i = 0; i < flecks; i++) {
			const float cx = next() * N, cy = next() * N;
			const float radius = 1.2f + next() * 1.0f;
			const float peak = 0.65f + next() * 0.35f;
			const int reach = static_cast<int>(std::ceil(radius));
			for (int dy = -reach; dy <= reach; dy++) {
				for (int dx = -reach; dx <= reach; dx++) {
					const int x = static_cast<int>(std::floor(cx)) + dx, y = static_cast<int>(std::floor(cy)) + dy;
					const float ddx = x + 0.5f - cx, ddy = y + 0.5f - cy;
					const float d = std::sqrt(ddx * ddx + ddy * ddy) / radius;
					if (d >= 1.0f) continue;
					auto& value = alpha[static_cast<size_t>(((y % N) + N) % N) * N + ((x % N) + N) % N];
					value = std::max(value, peak * (1.0f - d * d));
				}
			}
		}
		std::vector<uint8_t> out(alpha.size());
		for (size_t i = 0; i < alpha.size(); i++) out[i] = static_cast<uint8_t>(std::lround(std::clamp(alpha[i], 0.0f, 1.0f) * 255.0f));
		return out;
	}

	float Params::SparkleTile() const {
		return 75.0f * std::max(sparkleSize, 0.001f) * std::max(speed, 0.01f);
	}

	int Params::SparkleTextureSize() const {
		// A sparkle 3 pixels wide: side = 3 * tile / size (225 at speed 1)
		const float wanted = 3.0f * SparkleTile() / std::max(sparkleSize, 0.001f);
		int side = 128;
		while (side < 1024 && static_cast<float>(side) < wanted) side *= 2;
		return side;
	}

	std::vector<uint8_t> SparkleAlpha(const Params& params) {
		const int N = params.SparkleTextureSize();
		std::vector<uint8_t> alpha(static_cast<size_t>(N) * N, 0);
		const float radius = std::max(params.sparkleSize / params.SparkleTile() * static_cast<float>(N) * 0.5f, 0.75f);
		const float share = std::clamp(params.sparkleAmount, 0.0f, 100.0f) / 100.0f;
		const auto count = static_cast<uint32_t>(std::lround(share * static_cast<float>(N) * static_cast<float>(N) / (3.14159265f * radius * radius)));
		uint64_t state = 0x737061726B6C6500ull;
		const auto next = [&state] { return Unit(SplitMix(state++)); };
		const int reach = static_cast<int>(std::ceil(radius + 0.5f));
		for (uint32_t i = 0; i < count; i++) {
			const float cx = next() * N, cy = next() * N;
			for (int dy = -reach; dy <= reach; dy++) {
				for (int dx = -reach; dx <= reach; dx++) {
					const int x = static_cast<int>(std::floor(cx)) + dx, y = static_cast<int>(std::floor(cy)) + dy;
					const float ddx = x + 0.5f - cx, ddy = y + 0.5f - cy;
					// Flat, with a pixel's worth of edge
					const float cover = std::clamp(radius + 0.5f - std::sqrt(ddx * ddx + ddy * ddy), 0.0f, 1.0f);
					if (cover <= 0.0f) continue;
					auto& value = alpha[static_cast<size_t>(((y % N) + N) % N) * N + ((x % N) + N) % N];
					value = std::max(value, static_cast<uint8_t>(std::lround(cover * SPARKLE_ALPHA)));
				}
			}
		}
		return alpha;
	}

	std::vector<std::vector<uint8_t>> Mipmaps(const std::vector<uint8_t>& alpha, int keepPeaks) {
		std::vector<std::vector<uint8_t>> levels{ alpha };
		for (int size = Side(alpha) / 2, level = 0; size >= 1; size /= 2, level++) {
			const auto& above = levels.back();
			const int from = size * 2;
			std::vector<uint8_t> next(static_cast<size_t>(size) * size);
			for (int y = 0; y < size; y++) {
				for (int x = 0; x < size; x++) {
					const auto at = [&](int dx, int dy) { return static_cast<int>(above[static_cast<size_t>(y * 2 + dy) * from + x * 2 + dx]); };
					next[static_cast<size_t>(y) * size + x] = static_cast<uint8_t>(level < keepPeaks ? std::max({ at(0, 0), at(1, 0), at(0, 1), at(1, 1) }) :
						(at(0, 0) + at(1, 0) + at(0, 1) + at(1, 1) + 2) / 4);
				}
			}
			levels.push_back(std::move(next));
		}
		return levels;
	}

	uint32_t BrickSeed(uint64_t modelSeed, uint32_t brick) {
		const auto hash = SplitMix(SplitMix(modelSeed ^ 0x676C6974746572ull) + brick);
		return static_cast<uint32_t>(hash >> 32) | 1u;
	}

	glm::vec2 Uv(const glm::vec3& position, const glm::vec3& normal, float tile, uint32_t seed, eLayer layer) {
		const auto a = glm::abs(normal);
		const float scale = 1.0f / std::max(tile, 1e-3f);
		const int plane = a.x >= a.y && a.x >= a.z ? 0 : a.y >= a.z ? 1 : 2;
		const glm::vec2 uv = (plane == 0 ? glm::vec2(position.z, position.y) : plane == 1 ? glm::vec2(position.x, position.z) : glm::vec2(position.x, position.y)) * scale;
		if (seed == 0) return uv;
		const auto hash = SplitMix(((static_cast<uint64_t>(seed) << 2) | static_cast<uint64_t>(plane)) ^ (static_cast<uint64_t>(layer) << 40));
		const float angle = Unit(hash) * 6.28318530718f;
		const glm::vec2 offset(Unit(SplitMix(hash)), Unit(SplitMix(hash + 1)));
		const float c = std::cos(angle), s = std::sin(angle);
		return glm::vec2(c * uv.x - s * uv.y, s * uv.x + c * uv.y) + offset;
	}

	glm::vec4 SparkleColor(const glm::vec4& brickColor, const Params& params) {
		const float tint = std::clamp(params.sparkleTint, 0.0f, 100.0f) / 100.0f;
		const float brightness = std::clamp(params.sparkleBrightness, 0.0f, 100.0f) / 100.0f;
		return glm::vec4(glm::clamp(glm::mix(glm::vec3(1.0f), glm::vec3(brickColor), tint) * brightness, 0.0f, 1.0f), 1.0f);
	}

	float Sample(const std::vector<uint8_t>& alpha, const glm::vec2& uv) {
		const int N = Side(alpha);
		if (N == 0 || alpha.size() != static_cast<size_t>(N) * N) return 0.0f;
		const float x = (uv.x - std::floor(uv.x)) * N - 0.5f, y = (uv.y - std::floor(uv.y)) * N - 0.5f;
		const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
		const float fx = x - x0, fy = y - y0;
		const auto at = [&](int px, int py) { return alpha[static_cast<size_t>(((py % N) + N) % N) * N + ((px % N) + N) % N] / 255.0f; };
		return (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy) + (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy;
	}
}
