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


class CDMissionEmailTable : public CDTable<CDMissionEmailTable> {
private:
	std::vector<CDMissionEmail> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDMissionEmail> Query(std::function<bool(CDMissionEmail)> predicate);

	const std::vector<CDMissionEmail>& GetEntries() const;
};
