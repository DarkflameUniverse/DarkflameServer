#pragma once

// Custom Classes
#include "CDTable.h"

struct CDLevelProgressionLookup {
	uint32_t id;                    //!< The Level ID
	uint32_t requiredUScore;        //!< The required LEGO Score
	std::string BehaviorEffect;    //!< The behavior effect attached to this
};

class CDLevelProgressionLookupTable : public CDTable<CDLevelProgressionLookupTable> {
private:
	std::vector<CDLevelProgressionLookup> entries;

public:
	void LoadValuesFromDatabase();

	// Queries the table with a custom "where" clause
	std::vector<CDLevelProgressionLookup> Query(std::function<bool(CDLevelProgressionLookup)> predicate);

	const std::vector<CDLevelProgressionLookup>& GetEntries() const;
};
