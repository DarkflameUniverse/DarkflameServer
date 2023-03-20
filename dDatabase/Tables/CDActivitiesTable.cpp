#include "CDActivitiesTable.h"

CDActivitiesTable::CDActivitiesTable() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Activities");
	while (!tableData.eof()) {
		CDActivities entry;
		ActivityID activityId = tableData.getIntField("ActivityID", -1);
		UNUSED_COLUMN(entry.locStatus = tableData.getIntField("locStatus", -1));
		entry.instanceMapID = tableData.getIntField("instanceMapID", -1);
		entry.minTeams = tableData.getIntField("minTeams", -1);
		entry.maxTeams = tableData.getIntField("maxTeams", -1);
		entry.minTeamSize = tableData.getIntField("minTeamSize", -1);
		entry.maxTeamSize = tableData.getIntField("maxTeamSize", -1);
		entry.waitTime = tableData.getIntField("waitTime", -1);
		entry.startDelay = tableData.getIntField("startDelay", -1);
		UNUSED_COLUMN(entry.requiresUniqueData = tableData.getIntField("requiresUniqueData", -1));
		entry.leaderboardType = tableData.getIntField("leaderboardType", -1);
		UNUSED_COLUMN(entry.localize = tableData.getIntField("localize", -1));
		entry.optionalCostLOT = tableData.getIntField("optionalCostLOT", -1);
		entry.optionalCostCount = tableData.getIntField("optionalCostCount", -1);
		UNUSED_COLUMN(entry.showUIRewards = tableData.getIntField("showUIRewards", -1));
		UNUSED_COLUMN(entry.CommunityActivityFlagID = tableData.getIntField("CommunityActivityFlagID", -1));
		UNUSED_COLUMN(entry.gate_version = tableData.getStringField("gate_version", ""));
		entry.noTeamLootOnDeath = tableData.getIntField("noTeamLootOnDeath", -1);
		UNUSED_COLUMN(entry.optionalPercentage = tableData.getFloatField("optionalPercentage", -1.0f));

		auto insertedElement = this->entries.insert_or_assign(activityId, entry);
		DluAssert(insertedElement.second == true);

		tableData.nextRow();
	}
	tableData.finalize();
}

CDActivitiesResult CDActivitiesTable::GetActivity(ActivityID id) {
	const auto foundElement = this->entries.find(id);
	return foundElement != this->entries.end() ? CDActivitiesResult(foundElement->second) : CDActivitiesResult();
}
