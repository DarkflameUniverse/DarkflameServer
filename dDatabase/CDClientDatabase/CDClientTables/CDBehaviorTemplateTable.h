#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

struct CDBehaviorTemplate {
	uint32_t behaviorID;                                         //!< The Behavior ID
	uint32_t templateID;                                         //!< The Template ID (LOT)
	uint32_t effectID;                                           //!< The Effect ID attached
	std::unordered_set<std::string>::iterator effectHandle;          //!< The effect handle
};

class CDBehaviorTemplateTable : public CDTable<CDBehaviorTemplateTable, std::unordered_map<uint32_t, CDBehaviorTemplate>> {
public:
	void LoadValuesFromDatabase();

	const CDBehaviorTemplate GetByBehaviorID(uint32_t behaviorID);
};
