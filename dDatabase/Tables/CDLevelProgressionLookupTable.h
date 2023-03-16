#pragma once

// Custom Classes
#include "CDTable.h"

struct CDLevelProgressionLookup {
	unsigned int id;                    //!< The Level ID
	unsigned int requiredUScore;        //!< The required LEGO Score
	std::string BehaviorEffect;    //!< The behavior effect attached to this
};

class CDLevelProgressionLookupTable : public CDTable {
private:
	std::vector<CDLevelProgressionLookup> entries;

public:
	CDLevelProgressionLookupTable();

	static const std::string GetTableName() { return "LevelProgressionLookup"; };

	// Queries the table with a custom "where" clause
	std::vector<CDLevelProgressionLookup> Query(std::function<bool(CDLevelProgressionLookup)> predicate);

	// Gets all the entries in the table
	std::vector<CDLevelProgressionLookup> GetEntries(void) const;
};
