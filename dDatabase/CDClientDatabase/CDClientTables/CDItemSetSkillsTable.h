#pragma once

// Custom Classes
#include "CDTable.h"

struct CDItemSetSkills {
	uint32_t SkillSetID;            //!< The skill set ID
	uint32_t SkillID;               //!< The skill ID
	uint32_t SkillCastType;         //!< The skill cast type
};

class CDItemSetSkillsTable : public CDTable<CDItemSetSkillsTable> {
private:
	std::vector<CDItemSetSkills> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDItemSetSkills> Query(std::function<bool(CDItemSetSkills)> predicate);

	const std::vector<CDItemSetSkills>& GetEntries() const;

	std::vector<CDItemSetSkills> GetBySkillID(uint32_t SkillSetID);
};
