#include "CDActivitiesTable.h"

//! Constructor
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
		entry.ActivityID = tableData.getIntField(0, -1);
		entry.locStatus = tableData.getIntField(1, -1);
		entry.instanceMapID = tableData.getIntField(2, -1);
		entry.minTeams = tableData.getIntField(3, -1);
		entry.maxTeams = tableData.getIntField(4, -1);
		entry.minTeamSize = tableData.getIntField(5, -1);
		entry.maxTeamSize = tableData.getIntField(6, -1);
		entry.waitTime = tableData.getIntField(7, -1);
		entry.startDelay = tableData.getIntField(8, -1);
		entry.requiresUniqueData = tableData.getIntField(9, -1);
		entry.leaderboardType = tableData.getIntField(10, -1);
		entry.localize = tableData.getIntField(11, -1);
		entry.optionalCostLOT = tableData.getIntField(12, -1);
		entry.optionalCostCount = tableData.getIntField(13, -1);
		entry.showUIRewards = tableData.getIntField(14, -1);
		entry.CommunityActivityFlagID = tableData.getIntField(15, -1);
		entry.gate_version = tableData.getStringField(16, "");
		entry.noTeamLootOnDeath = tableData.getIntField(17, -1);
		entry.optionalPercentage = tableData.getFloatField(18, -1.0f);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDActivitiesTable::~CDActivitiesTable(void) {}

//! Returns the table's name
std::string CDActivitiesTable::GetName(void) const {
	return "Activities";
}

//! Queries the table with a custom "where" clause
std::vector<CDActivities> CDActivitiesTable::Query(std::function<bool(CDActivities)> predicate) {

	std::vector<CDActivities> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDActivities> CDActivitiesTable::GetEntries(void) const {
	return this->entries;
}
