#pragma once

// Custom Classes
#include "CDTable.h"

struct CDLootTable {
	unsigned int itemid;                 //!< The LOT of the item
	unsigned int LootTableIndex;         //!< The Loot Table Index
	bool MissionDrop;               //!< Whether or not this loot table is a mission drop
	unsigned int sortPriority;           //!< The sorting priority
};

typedef uint32_t LootTableIndex;
typedef std::vector<CDLootTable> LootTableEntries;

class CDLootTableTable : public CDTable<CDLootTableTable> {
private:
	std::unordered_map<LootTableIndex, LootTableEntries> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	const LootTableEntries& GetTable(uint32_t tableId);
};

