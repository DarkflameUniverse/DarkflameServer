#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

#include "dCommonVars.h"

/**
 * Which of the positions world servers report every second are kept for replays: a player's first position in a world,
 * then one every `interval` seconds while they move, and one every `idleInterval` seconds while they stand still (so a
 * replay knows they are still there). Pure, so it can be unit tested.
 */
namespace PositionHistory {
	// Moving less than this since the last kept sample counts as standing still
	constexpr float MIN_MOVE = 0.5f;

	class Throttle {
	public:
		struct Last {
			int64_t time{};
			uint32_t zone{};
			uint32_t instance{};
			float x{}, y{}, z{};
		};

		// Whether to keep this report; remembers it when it is kept
		bool Keep(LWOOBJID character, uint32_t zone, uint32_t instance, float x, float y, float z, int64_t now, int64_t interval, int64_t idleInterval) {
			const auto it = m_Last.find(character);
			bool keep = it == m_Last.end() || it->second.zone != zone || it->second.instance != instance || now < it->second.time;
			if (!keep) {
				const auto& last = it->second;
				const auto elapsed = now - last.time;
				const auto moved = std::hypot(x - last.x, y - last.y, z - last.z);
				keep = (elapsed >= interval && moved >= MIN_MOVE) || elapsed >= std::max(idleInterval, interval);
			}
			if (keep) m_Last[character] = { now, zone, instance, x, y, z };
			return keep;
		}

		// Forget players not kept since `before` (they left), so the map doesn't grow forever
		void Forget(int64_t before) {
			std::erase_if(m_Last, [before](const auto& entry) { return entry.second.time < before; });
		}

		size_t Size() const { return m_Last.size(); }

	private:
		std::unordered_map<LWOOBJID, Last> m_Last;
	};

	// Bucket size for a replay over `span` seconds so each player has at most about `maxPerPlayer` samples; 1 (every
	// sample) when the samples kept every `interval` seconds are few enough already
	inline int64_t BucketFor(int64_t span, int64_t interval, int64_t maxPerPlayer) {
		const auto needed = (span + maxPerPlayer - 1) / std::max<int64_t>(maxPerPlayer, 1);
		return needed <= std::max<int64_t>(interval, 1) ? 1 : needed;
	}
}
