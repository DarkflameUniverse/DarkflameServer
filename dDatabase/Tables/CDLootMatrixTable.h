#pragma once

// Custom Classes
#include "CDTable.h"

struct CDLootMatrix {
	unsigned int LootMatrixIndex;           //!< The Loot Matrix Index
	unsigned int LootTableIndex;            //!< The Loot Table Index
	unsigned int RarityTableIndex;          //!< The Rarity Table Index
	float percent;                   //!< The percent that this matrix is used?
	unsigned int minToDrop;                 //!< The minimum amount of loot from this matrix to drop
	unsigned int maxToDrop;                 //!< The maximum amount of loot from this matrix to drop
	unsigned int id;                        //!< The ID of the Loot Matrix
	unsigned int flagID;                    //!< ???
	UNUSED(std::string gate_version);          //!< The Gate Version
};

namespace CDLootMatrixTable {
private:
	std::vector<CDLootMatrix> entries;

public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDLootMatrix> Query(std::function<bool(CDLootMatrix)> predicate);

	const std::vector<CDLootMatrix>& GetEntries(void) const;
};

