#include "UgcGlitter.h"

#include <algorithm>
#include <cmath>

namespace UgcGlitter {
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

	std::vector<std::vector<uint8_t>> Mipmaps(const std::vector<uint8_t>& alpha) {
		std::vector<std::vector<uint8_t>> levels{ alpha };
		for (int size = TEXTURE_SIZE / 2; size >= 1; size /= 2) {
			const auto& above = levels.back();
			const int from = size * 2;
			std::vector<uint8_t> level(static_cast<size_t>(size) * size);
			for (int y = 0; y < size; y++) {
				for (int x = 0; x < size; x++) {
					const auto at = [&](int dx, int dy) { return static_cast<int>(above[static_cast<size_t>(y * 2 + dy) * from + x * 2 + dx]); };
					level[static_cast<size_t>(y) * size + x] = static_cast<uint8_t>((at(0, 0) + at(1, 0) + at(0, 1) + at(1, 1) + 2) / 4);
				}
			}
			levels.push_back(std::move(level));
		}
		return levels;
	}

	glm::vec2 Uv(const glm::vec3& position, const glm::vec3& normal, float tile) {
		const auto a = glm::abs(normal);
		const float scale = 1.0f / std::max(tile, 1e-3f);
		if (a.x >= a.y && a.x >= a.z) return glm::vec2(position.z, position.y) * scale;
		if (a.y >= a.z) return glm::vec2(position.x, position.z) * scale;
		return glm::vec2(position.x, position.y) * scale;
	}

	float Sample(const std::vector<uint8_t>& alpha, const glm::vec2& uv) {
		constexpr int N = TEXTURE_SIZE;
		if (alpha.size() != static_cast<size_t>(N) * N) return 0.0f;
		const float x = (uv.x - std::floor(uv.x)) * N - 0.5f, y = (uv.y - std::floor(uv.y)) * N - 0.5f;
		const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
		const float fx = x - x0, fy = y - y0;
		const auto at = [&](int px, int py) { return alpha[static_cast<size_t>(((py % N) + N) % N) * N + ((px % N) + N) % N] / 255.0f; };
		return (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy) + (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy;
	}
}
