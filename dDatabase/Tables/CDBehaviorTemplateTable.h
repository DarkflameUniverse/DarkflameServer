#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

struct CDBehaviorTemplate {
	uint32_t behaviorID;										//!< The Behavior ID
	uint32_t templateID;										//!< The Template ID (LOT)
	uint32_t effectID;  										//!< The Effect ID attached
	std::unordered_set<std::string>::iterator effectHandle;		//!< The effect handle
};


namespace CDBehaviorTemplateTable {
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDBehaviorTemplate> Query(std::function<bool(CDBehaviorTemplate)> predicate);

	const CDBehaviorTemplate GetByBehaviorID(uint32_t behaviorID);
};
