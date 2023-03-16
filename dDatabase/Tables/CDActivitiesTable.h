#pragma once

// Custom Classes
#include "CDTable.h"

struct CDActivities {
	unsigned int ActivityID;
	unsigned int locStatus;
	unsigned int instanceMapID;
	unsigned int minTeams;
	unsigned int maxTeams;
	unsigned int minTeamSize;
	unsigned int maxTeamSize;
	unsigned int waitTime;
	unsigned int startDelay;
	bool requiresUniqueData;
	unsigned int leaderboardType;
	bool localize;
	int optionalCostLOT;
	int optionalCostCount;
	bool showUIRewards;
	unsigned int CommunityActivityFlagID;
	std::string gate_version;
	bool noTeamLootOnDeath;
	float optionalPercentage;
};

class CDActivitiesTable : public CDTable {
private:
	std::vector<CDActivities> entries;

public:
	CDActivitiesTable();

	static const std::string GetTableName() { return "Activities"; };

	// Queries the table with a custom "where" clause
	std::vector<CDActivities> Query(std::function<bool(CDActivities)> predicate);

	std::vector<CDActivities> GetEntries(void) const;
};
