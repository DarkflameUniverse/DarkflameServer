#pragma once

// Custom Classes
#include "CDTable.h"

struct CDItemSets {
	uint32_t setID;             //!< The item set ID
	uint32_t locStatus;         //!< The loc status
	std::string itemIDs;       //!< THe item IDs
	uint32_t kitType;           //!< The item kit type
	uint32_t kitRank;           //!< The item kit rank
	uint32_t kitImage;          //!< The item kit image
	uint32_t skillSetWith2;     //!< The skill set with 2
	uint32_t skillSetWith3;     //!< The skill set with 3
	uint32_t skillSetWith4;     //!< The skill set with 4
	uint32_t skillSetWith5;     //!< The skill set with 5
	uint32_t skillSetWith6;     //!< The skill set with 6
	bool localize;          //!< Whether or localize
	std::string gate_version;  //!< The gate version
	uint32_t kitID;             //!< The kit ID
	float priority;         //!< The priority
};

class CDItemSetsTable : public CDTable<CDItemSetsTable, std::vector<CDItemSets>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDItemSets> Query(std::function<bool(CDItemSets)> predicate);
};

