#pragma once

// Custom Classes
#include "CDTable.h"

struct CDRarityTable {
	float randmax;
	uint32_t rarity;
};

typedef std::vector<CDRarityTable> RarityTable;

class CDRarityTableTable : public CDTable<CDRarityTableTable> {
private:
	typedef uint32_t RarityTableIndex;
	std::unordered_map<RarityTableIndex, std::vector<CDRarityTable>> entries;

public:
	void LoadValuesFromDatabase();

	const std::vector<CDRarityTable>& GetRarityTable(uint32_t predicate);
};

