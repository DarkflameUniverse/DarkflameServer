#pragma once

// Custom Classes
#include "CDTable.h"

struct CDObjectSkills {
	unsigned int objectTemplate;        //!< The LOT of the item
	unsigned int skillID;               //!< The Skill ID of the object
	unsigned int castOnType;            //!< ???
	unsigned int AICombatWeight;        //!< ???
};

namespace CDObjectSkillsTable {
private:
	std::vector<CDObjectSkills> entries;

public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDObjectSkills> Query(std::function<bool(CDObjectSkills)> predicate);

	// Gets all the entries in the table
	std::vector<CDObjectSkills> GetEntries(void) const;

};

