#pragma once

#include "dCommonVars.h"
#include <unordered_map>
#include "Singleton.h"
#include <vector>

class Entity;

struct RarityTableEntry {
	uint32_t rarity;
	float randMax;
};

typedef std::vector<RarityTableEntry> RarityTable;

struct LootMatrixEntry {
	uint32_t lootTableIndex;
	uint32_t rarityTableIndex;
	float percent;
	uint32_t minDrop;
	uint32_t maxDrop;
};

typedef std::vector<LootMatrixEntry> LootMatrix;

struct LootTableEntry {
	LOT itemID;
	bool isMissionDrop;
};

typedef std::vector<LootTableEntry> LootTable;

// used for glue code with Entity and Player classes
namespace Loot {
	struct Info {
		LWOOBJID id;
		LOT lot;
		uint32_t count;
	};
}


class LootGenerator : public Singleton<LootGenerator> {
public:
	LootGenerator();

	std::unordered_map<LOT, int32_t> RollLootMatrix(Entity* player, uint32_t matrixIndex);
	std::unordered_map<LOT, int32_t> RollLootMatrix(uint32_t matrixIndex);
	void GiveLoot(Entity* player, uint32_t matrixIndex, eLootSourceType lootSourceType = eLootSourceType::LOOT_SOURCE_NONE);
	void GiveLoot(Entity* player, std::unordered_map<LOT, int32_t>& result, eLootSourceType lootSourceType = eLootSourceType::LOOT_SOURCE_NONE);
	void GiveActivityLoot(Entity* player, Entity* source, uint32_t activityID, int32_t rating = 0);
	void DropLoot(Entity* player, Entity* killedObject, uint32_t matrixIndex, uint32_t minCoins, uint32_t maxCoins);
	void DropLoot(Entity* player, Entity* killedObject, std::unordered_map<LOT, int32_t>& result, uint32_t minCoins, uint32_t maxCoins);
	void DropActivityLoot(Entity* player, Entity* source, uint32_t activityID, int32_t rating = 0);

private:
	std::unordered_map<uint32_t, uint8_t> m_ItemRarities;
	std::unordered_map<uint32_t, RarityTable> m_RarityTables;
	std::unordered_map<uint32_t, LootMatrix> m_LootMatrices;
	std::unordered_map<uint32_t, LootTable> m_LootTables;
};
