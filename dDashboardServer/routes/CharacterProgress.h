#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

/**
 * A character's completion on the character page: missions and achievements done out of those in the game, per group
 * the game sorts them into and per zone (with the zone's own statistics and the collectibles its summary tracks), and
 * how that compares with everyone else on the server. The server's averages come from reading every character, so
 * they are worked out in the background and kept for a few hours.
 */
void RegisterCharacterProgressRoutes();

namespace CharacterProgress {
	struct Standing {
		double average{};
		double percentile{}; // share of the others with fewer, 0-100
	};

	// Where `value` stands among every character's count (sorted ascending)
	inline Standing Compare(const std::vector<uint32_t>& sorted, uint32_t value) {
		Standing standing;
		if (sorted.empty()) return standing;
		uint64_t total = 0;
		for (const auto count : sorted) total += count;
		standing.average = static_cast<double>(total) / static_cast<double>(sorted.size());
		const auto below = std::lower_bound(sorted.begin(), sorted.end(), value) - sorted.begin();
		standing.percentile = 100.0 * static_cast<double>(below) / static_cast<double>(sorted.size());
		return standing;
	}
}
