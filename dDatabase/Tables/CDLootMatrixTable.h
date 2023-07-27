#pragma once

// Custom Classes
#include "CDTable.h"

struct CDLootMatrix {
	unsigned int LootTableIndex;            //!< The Loot Table Index
	unsigned int RarityTableIndex;          //!< The Rarity Table Index
	float percent;                   //!< The percent that this matrix is used?
	unsigned int minToDrop;                 //!< The minimum amount of loot from this matrix to drop
	unsigned int maxToDrop;                 //!< The maximum amount of loot from this matrix to drop
	unsigned int flagID;                    //!< ???
	UNUSED(std::string gate_version);          //!< The Gate Version
};

typedef uint32_t LootMatrixIndex;
typedef std::vector<CDLootMatrix> LootMatrixEntries;

class CDLootMatrixTable : public CDTable<CDLootMatrixTable> {
public:
	void LoadValuesFromDatabase();

	// Gets a matrix by ID or inserts a blank one if none existed.
	const LootMatrixEntries& GetMatrix(uint32_t matrixId);
private:
	std::unordered_map<LootMatrixIndex, LootMatrixEntries> entries;
};

