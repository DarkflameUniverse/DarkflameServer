#pragma once

// Custom Classes
#include "CDTable.h"

#include <cstdint>

struct CDMissionEmail {
	uint32_t ID;
	uint32_t messageType;
	uint32_t notificationGroup;
	uint32_t missionID;
	uint32_t attachmentLOT;
	bool localize;
	uint32_t locStatus;
	std::string gate_version;
};


class CDMissionEmailTable : public CDTable<CDMissionEmailTable> {
private:
	std::vector<CDMissionEmail> m_Entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDMissionEmail> Query(std::function<bool(CDMissionEmail)> predicate);

	const std::vector<CDMissionEmail>& GetEntries() const;
};
