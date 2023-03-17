#include "CDActivitiesTable.h"

CDActivitiesTable::CDActivitiesTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM Activities");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Activities");
	while (!tableData.eof()) {
		CDActivities entry;
		entry.ActivityID = tableData.getIntField("ActivityID", -1);
		entry.locStatus = tableData.getIntField("locStatus", -1);
		entry.instanceMapID = tableData.getIntField("instanceMapID", -1);
		entry.minTeams = tableData.getIntField("minTeams", -1);
		entry.maxTeams = tableData.getIntField("maxTeams", -1);
		entry.minTeamSize = tableData.getIntField("minTeamSize", -1);
		entry.maxTeamSize = tableData.getIntField("maxTeamSize", -1);
		entry.waitTime = tableData.getIntField("waitTime", -1);
		entry.startDelay = tableData.getIntField("startDelay", -1);
		entry.requiresUniqueData = tableData.getIntField("requiresUniqueData", -1);
		entry.leaderboardType = tableData.getIntField("leaderboardType", -1);
		entry.localize = tableData.getIntField("localize", -1);
		entry.optionalCostLOT = tableData.getIntField("optionalCostLOT", -1);
		entry.optionalCostCount = tableData.getIntField("optionalCostCount", -1);
		entry.showUIRewards = tableData.getIntField("showUIRewards", -1);
		entry.CommunityActivityFlagID = tableData.getIntField("CommunityActivityFlagID", -1);
		entry.gate_version = tableData.getStringField("gate_version", "");
		entry.noTeamLootOnDeath = tableData.getIntField("noTeamLootOnDeath", -1);
		entry.optionalPercentage = tableData.getFloatField("optionalPercentage", -1.0f);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDActivities> CDActivitiesTable::Query(std::function<bool(CDActivities)> predicate) {

	std::vector<CDActivities> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

std::vector<CDActivities> CDActivitiesTable::GetEntries(void) const {
	return this->entries;
}

