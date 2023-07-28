#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

struct CDBehaviorTemplate {
    unsigned int behaviorID;                                         //!< The Behavior ID
    unsigned int templateID;                                         //!< The Template ID (LOT)
    unsigned int effectID;                                           //!< The Effect ID attached
    size_t effectHandle;                                             //!< The effect handle
};


class CDBehaviorTemplateTable : public CDTable<CDBehaviorTemplateTable> {
private:
	std::vector<CDBehaviorTemplate> entries;
	std::unordered_map<uint32_t, CDBehaviorTemplate> entriesMappedByBehaviorID;
	std::unordered_set<std::string> m_EffectHandles;
public:
	CDBehaviorTemplateTable();
	// Queries the table with a custom "where" clause
	std::vector<CDBehaviorTemplate> Query(std::function<bool(CDBehaviorTemplate)> predicate);

	std::vector<CDBehaviorTemplate> GetEntries(void) const;

	const CDBehaviorTemplate GetByBehaviorID(uint32_t behaviorID);
};
