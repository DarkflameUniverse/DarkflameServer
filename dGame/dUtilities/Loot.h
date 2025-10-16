#pragma once

#include "dCommonVars.h"
#include "eLootSourceType.h"
#include <unordered_map>

class Entity;

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
};
