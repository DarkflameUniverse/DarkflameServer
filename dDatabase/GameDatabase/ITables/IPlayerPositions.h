#ifndef __IPLAYERPOSITIONS__H__
#define __IPLAYERPOSITIONS__H__

#include <cstdint>
#include <optional>
#include <vector>

#include "dCommonVars.h"
#include "json.hpp"

/**
 * Where players were: position samples the dashboard takes every few seconds from the positions world servers report,
 * for replaying movement on the 3D world view (staff only, kept for position_history_days). Also the per-day map
 * event cells behind the heat map timelapse.
 */
class IPlayerPositions {
public:
	struct PositionSample {
		int64_t time{};
		LWOOBJID characterId{};
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
		float x{};
		float y{};
		float z{};
	};

	// Adds a batch in one transaction; a character's second sample in the same second replaces the first
	virtual void InsertPositionSamples(const std::vector<PositionSample>& samples) = 0;

	/**
	 * Samples in a zone between from and to (instance 0: every instance), ordered by character and time. With
	 * bucketSeconds > 1 each character gets one sample per bucket and instance (its average position, at the bucket's
	 * first time). At most `limit` rows.
	 */
	virtual std::vector<PositionSample> GetPositionSamples(uint32_t zoneId, uint32_t instanceId, int64_t from, int64_t to, int64_t bucketSeconds, uint32_t limit) = 0;

	struct PositionInstance {
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
		int64_t first{};
		int64_t last{};
		uint32_t players{};
	};

	// World instances with samples between from and to (zone 0: every zone), with their first and last sample and players seen
	virtual std::vector<PositionInstance> GetPositionInstances(uint32_t zoneId, int64_t from, int64_t to) = 0;

	virtual uint32_t PrunePositionSamples(int64_t beforeTime) = 0;

	// Map events of a kind (IEconomyLedger::eMapEvent) per day and cell for a zone (one clone, or every instance):
	// [{day, x, z, events}], by day, at most `limit`
	virtual nlohmann::json GetMapCellsPerDay(uint32_t zoneId, std::optional<uint32_t> cloneId, uint8_t kind, uint32_t fromDay, uint32_t toDay, uint32_t limit) = 0;

	// Characters seen in a zone's clone (a property) between from and to: [{character_id, name, first, last}], latest first
	virtual nlohmann::json GetCloneVisitors(uint32_t zoneId, uint32_t cloneId, int64_t from, int64_t to, uint32_t limit) = 0;
};

#endif  //!__IPLAYERPOSITIONS__H__
