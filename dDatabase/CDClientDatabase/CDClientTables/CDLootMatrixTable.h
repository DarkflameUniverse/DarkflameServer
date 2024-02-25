#pragma once

// Custom Classes
#include "CDTable.h"

struct CDLootMatrix {
	uint32_t LootTableIndex;            //!< The Loot Table Index
	uint32_t RarityTableIndex;          //!< The Rarity Table Index
	float percent;                   //!< The percent that this matrix is used?
	uint32_t minToDrop;                 //!< The minimum amount of loot from this matrix to drop
	uint32_t maxToDrop;                 //!< The maximum amount of loot from this matrix to drop
	uint32_t flagID;                    //!< ???
	UNUSED(std::string gate_version);          //!< The Gate Version
};

typedef uint32_t LootMatrixIndex;
typedef std::vector<CDLootMatrix> LootMatrixEntries;

class CDLootMatrixTable : public CDTable<CDLootMatrixTable, std::unordered_map<LootMatrixIndex, LootMatrixEntries>> {
public:
	void LoadValuesFromDatabase();

	// Gets a matrix by ID or inserts a blank one if none existed.
	const LootMatrixEntries& GetMatrix(uint32_t matrixId);
private:
	CDLootMatrix ReadRow(CppSQLite3Query& tableData) const;
};

