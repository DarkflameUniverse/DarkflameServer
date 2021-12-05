#pragma once
#include "dCommonVars.h"
#include <vector>
#include "CDClientManager.h"
class Entity;

namespace Loot {
	struct Info {
		LWOOBJID id;
		LOT lot;
		uint32_t count;
	};

	std::vector<CDLootTable> GetLootOfRarity(const std::vector<CDLootTable> &lootTable, uint32_t rarity);

	void DropActivityLoot(Entity* user, Entity* source, uint32_t activityID, int32_t rating = 0);

	void GiveActivityLoot(Entity* user, Entity* source, uint32_t activityID, int32_t rating = 0);

	void CalculateLootMatrix(uint32_t lootMatrixID, Entity* user, std::unordered_map<LOT, int32_t>& result);

	void GiveLoot(Entity* user, uint32_t lootMatrixID);

	void DropLoot(Entity* user, Entity* killedObject, uint32_t lootMatrixID, uint32_t minCoins, uint32_t maxCoins);

	void GiveLoot(Entity* user, std::unordered_map<LOT, int32_t>& result);

	void DropLoot(Entity* user, Entity* killedObject, std::unordered_map<LOT, int32_t>& result, uint32_t minCoins, uint32_t maxCoins);

	void DropItem(Entity* user, Entity* sourceObject, LOT item, int32_t currency, int32_t count, bool useTeam = false, bool freeForAll = false);
};