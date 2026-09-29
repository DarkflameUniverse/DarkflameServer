#pragma once

#include "dCommonVars.h"
#include "eLootSourceType.h"
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

class Entity;
struct CDActivityRewards;
struct CDCurrencyTable;

namespace GameMessages {
	struct DropClientLoot;
};

namespace Loot {
	struct Info {
		LWOOBJID id = 0;
		LOT lot = 0;
		int32_t count = 0;
	};

	using Return = std::map<LOT, int32_t>;

	Loot::Return RollLootMatrix(Entity* player, uint32_t matrixIndex);
	void CacheMatrix(const uint32_t matrixIndex);
	void GiveLoot(Entity* player, uint32_t matrixIndex, eLootSourceType lootSourceType = eLootSourceType::NONE);
	void GiveLoot(Entity* player, const Loot::Return& result, eLootSourceType lootSourceType = eLootSourceType::NONE);
	void GiveActivityLoot(Entity* player, const LWOOBJID source, uint32_t activityID, int32_t rating = 0);
	void DropLoot(Entity* player, const LWOOBJID source, uint32_t matrixIndex, uint32_t minCoins, uint32_t maxCoins);
	void DropItem(Entity& player, GameMessages::DropClientLoot& lootMsg, bool useTeam = false, bool forceFfa = false);
	void DropActivityLoot(Entity* player, const LWOOBJID source, uint32_t activityID, int32_t rating = 0);

	/**
	 * The coin range {min, max} of an ActivityRewards row, as live rolled it: the reward's ChallengeRating is the
	 * CurrencyTable npcminlevel, and level 1 is used when the currency index has no row for that level.
	 * currencyRows are the CurrencyTable rows of the reward's CurrencyIndex. {0, 0} when neither level has a row.
	 */
	std::pair<uint32_t, uint32_t> GetActivityCoinRange(const std::vector<CDCurrencyTable>& currencyRows, uint32_t challengeRating);
	std::pair<uint32_t, uint32_t> GetActivityCoinRange(const CDActivityRewards& reward);

	// How far from where it spawns a dropped item lands, as live dropped them (median 10.0 units, uniform direction).
	constexpr float ITEM_DROP_DISTANCE = 10.0f;

	/**
	 * Fills DropClientLoot's use_position and final_position the way live did (12,501 live DropClientLoot).
	 * use_position is set only when a player is the source (activity rewards such as the dragon and BONS chests):
	 * otherwise the client spawns the loot at the source object where it is on the client, and only falls back
	 * to the spawn position if that object is gone. Coins land where they spawn; items land ITEM_DROP_DISTANCE away
	 * at the given angle (radians, around Y).
	 * Nothing is set when there is no spawn position.
	 */
	void SetDropPositions(GameMessages::DropClientLoot& lootMsg, bool sourceIsPlayer, float angle);
};
