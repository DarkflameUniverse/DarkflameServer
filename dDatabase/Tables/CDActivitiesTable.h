#pragma once

#include "CDTable.h"

typedef uint32_t ActivityID;

struct CDActivities {
	UNUSED_COLUMN(uint32_t locStatus);
	uint32_t instanceMapID;
	uint32_t minTeams;
	uint32_t maxTeams;
	uint32_t minTeamSize;
	uint32_t maxTeamSize;
	uint32_t waitTime;
	uint32_t startDelay;
	UNUSED_COLUMN(bool requiresUniqueData);
	uint32_t leaderboardType;
	UNUSED_COLUMN(bool localize);
	int32_t optionalCostLOT;
	int32_t optionalCostCount;
	UNUSED_COLUMN(bool showUIRewards);
	UNUSED_COLUMN(uint32_t CommunityActivityFlagID);
	UNUSED_COLUMN(std::string gate_version);
	bool noTeamLootOnDeath;
	UNUSED_COLUMN(float optionalPercentage);
};
typedef LookupResult<CDActivities> CDActivitiesResult;

class CDActivitiesTable : public CDTable<CDActivitiesTable> {
private:
	std::map<ActivityID, CDActivities> entries;

public:
	CDActivitiesTable();
	// Queries the table with a custom "where" clause
	CDActivitiesResult GetActivity(ActivityID predicate);
};
