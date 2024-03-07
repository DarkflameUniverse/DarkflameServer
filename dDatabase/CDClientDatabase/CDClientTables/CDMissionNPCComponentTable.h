#pragma once

// Custom Classes
#include "CDTable.h"

#include <cstdint>

struct CDMissionNPCComponent {
	uint32_t id;                //!< The ID
	uint32_t missionID;         //!< The Mission ID
	bool offersMission;     //!< Whether or not this NPC offers a mission
	bool acceptsMission;    //!< Whether or not this NPC accepts a mission
	std::string gate_version;  //!< The gate version
};

class CDMissionNPCComponentTable : public CDTable<CDMissionNPCComponentTable, std::vector<CDMissionNPCComponent>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDMissionNPCComponent> Query(std::function<bool(CDMissionNPCComponent)> predicate);
};
