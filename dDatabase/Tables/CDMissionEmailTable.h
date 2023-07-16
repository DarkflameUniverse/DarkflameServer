#pragma once

// Custom Classes
#include "CDTable.h"

struct CDMissionEmail {
	unsigned int ID;
	unsigned int messageType;
	unsigned int notificationGroup;
	unsigned int missionID;
	unsigned int attachmentLOT;
	bool localize;
	unsigned int locStatus;
	std::string gate_version;
};


namespace CDMissionEmailTable {
private:
	std::vector<CDMissionEmail> entries;

public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDMissionEmail> Query(std::function<bool(CDMissionEmail)> predicate);

	std::vector<CDMissionEmail> GetEntries(void) const;
};
