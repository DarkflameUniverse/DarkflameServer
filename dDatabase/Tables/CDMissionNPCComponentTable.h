#pragma once

// Custom Classes
#include "CDTable.h"

struct CDMissionNPCComponent {
	unsigned int id;                //!< The ID
	unsigned int missionID;         //!< The Mission ID
	bool offersMission;     //!< Whether or not this NPC offers a mission
	bool acceptsMission;    //!< Whether or not this NPC accepts a mission
	std::string gate_version;  //!< The gate version
};

class CDMissionNPCComponentTable : public CDTable {
private:
	std::vector<CDMissionNPCComponent> entries;

public:
	CDMissionNPCComponentTable();

	static const std::string GetTableName() { return "MissionNPCComponent"; };

	// Queries the table with a custom "where" clause
	std::vector<CDMissionNPCComponent> Query(std::function<bool(CDMissionNPCComponent)> predicate);

	// Gets all the entries in the table
	std::vector<CDMissionNPCComponent> GetEntries(void) const;

};

