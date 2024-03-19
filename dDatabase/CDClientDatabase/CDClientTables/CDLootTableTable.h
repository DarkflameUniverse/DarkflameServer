#pragma once

// Custom Classes
#include "CDTable.h"

#include <cstdint>

struct CDLootTable {
	uint32_t itemid;                 //!< The LOT of the item
	uint32_t LootTableIndex;         //!< The Loot Table Index
	bool MissionDrop;               //!< Whether or not this loot table is a mission drop
	uint32_t sortPriority;           //!< The sorting priority
};

typedef uint32_t LootTableIndex;
typedef std::vector<CDLootTable> LootTableEntries;

class CDLootTableTable : public CDTable<CDLootTableTable, std::unordered_map<LootTableIndex, LootTableEntries>> {
private:
	CDLootTable ReadRow(CppSQLite3Query& tableData) const;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	const LootTableEntries& GetTable(const uint32_t tableId);
};
