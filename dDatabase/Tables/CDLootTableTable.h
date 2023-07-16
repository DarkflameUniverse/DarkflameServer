#pragma once

// Custom Classes
#include "CDTable.h"

struct CDLootTable {
	unsigned int itemid;                 //!< The LOT of the item
	unsigned int LootTableIndex;         //!< The Loot Table Index
	unsigned int id;                     //!< The ID
	bool MissionDrop;               //!< Whether or not this loot table is a mission drop
	unsigned int sortPriority;           //!< The sorting priority
};

namespace CDLootTableTable {
private:
	std::vector<CDLootTable> entries;

public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDLootTable> Query(std::function<bool(CDLootTable)> predicate);

	const std::vector<CDLootTable>& GetEntries(void) const;
};

