#pragma once

// Custom Classes
#include "CDTable.h"

struct CDActivities {
	uint32_t ActivityID;
	uint32_t locStatus;
	uint32_t instanceMapID;
	uint32_t minTeams;
	uint32_t maxTeams;
	uint32_t minTeamSize;
	uint32_t maxTeamSize;
	uint32_t waitTime;
	uint32_t startDelay;
	bool requiresUniqueData;
	uint32_t leaderboardType;
	bool localize;
	int32_t optionalCostLOT;
	int32_t optionalCostCount;
	bool showUIRewards;
	uint32_t CommunityActivityFlagID;
	std::string gate_version;
	bool noTeamLootOnDeath;
	float optionalPercentage;
};

class CDActivitiesTable : public CDTable<CDActivitiesTable, std::vector<CDActivities>> {
public:
	void LoadValuesFromDatabase();

	// Queries the table with a custom "where" clause
	std::vector<CDActivities> Query(std::function<bool(CDActivities)> predicate);
};
