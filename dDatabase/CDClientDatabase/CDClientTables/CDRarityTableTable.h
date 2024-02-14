#pragma once

// Custom Classes
#include "CDTable.h"

struct CDRarityTable {
	float randmax;
	uint32_t rarity;

	typedef uint32_t Index;
};

typedef std::vector<CDRarityTable> RarityTable;

class CDRarityTableTable : public CDTable<CDRarityTableTable, std::unordered_map<CDRarityTable::Index, std::vector<CDRarityTable>>> {
public:
	void LoadValuesFromDatabase();

	const std::vector<CDRarityTable>& GetRarityTable(uint32_t predicate);
};

