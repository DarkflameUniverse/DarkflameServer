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

class CDMissionNPCComponentTable : public CDTable<CDMissionNPCComponentTable> {
private:
	std::vector<CDMissionNPCComponent> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDMissionNPCComponent> Query(std::function<bool(CDMissionNPCComponent)> predicate);

	// Gets all the entries in the table
	const std::vector<CDMissionNPCComponent>& GetEntries() const;

};

