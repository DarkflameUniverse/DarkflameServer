#pragma once

// Custom Classes
#include "CDTable.h"

struct CDItemSetSkills {
	unsigned int SkillSetID;            //!< The skill set ID
	unsigned int SkillID;               //!< The skill ID
	unsigned int SkillCastType;         //!< The skill cast type
};

class CDItemSetSkillsTable : public CDTable<CDItemSetSkillsTable> {
private:
	std::vector<CDItemSetSkills> entries;

public:
	CDItemSetSkillsTable();
	// Queries the table with a custom "where" clause
	std::vector<CDItemSetSkills> Query(std::function<bool(CDItemSetSkills)> predicate);

	std::vector<CDItemSetSkills> GetEntries(void) const;

	std::vector<CDItemSetSkills> GetBySkillID(unsigned int SkillSetID);
};
