#pragma once

#include "dCommonVars.h"
#include <unordered_map>

class Entity;

namespace Loot {
	struct Info {
		LWOOBJID id = 0;
		LOT lot = 0;
		uint32_t count = 0;
	};

	std::unordered_map<LOT, int32_t> RollLootMatrix(Entity* player, uint32_t matrixIndex);
	std::unordered_map<LOT, int32_t> RollLootMatrix(uint32_t matrixIndex);
	void CacheMatrix(const uint32_t matrixIndex);
	void GiveLoot(Entity* player, uint32_t matrixIndex, eLootSourceType lootSourceType = eLootSourceType::NONE);
	void GiveLoot(Entity* player, std::unordered_map<LOT, int32_t>& result, eLootSourceType lootSourceType = eLootSourceType::NONE);
	void GiveActivityLoot(Entity* player, Entity* source, uint32_t activityID, int32_t rating = 0);
	void DropLoot(Entity* player, Entity* killedObject, uint32_t matrixIndex, uint32_t minCoins, uint32_t maxCoins);
	void DropLoot(Entity* player, Entity* killedObject, std::unordered_map<LOT, int32_t>& result, uint32_t minCoins, uint32_t maxCoins);
	void DropActivityLoot(Entity* player, Entity* source, uint32_t activityID, int32_t rating = 0);
};
