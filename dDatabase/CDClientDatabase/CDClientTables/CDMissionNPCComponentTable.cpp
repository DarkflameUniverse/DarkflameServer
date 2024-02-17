#include "CDMissionNPCComponentTable.h"

void CDMissionNPCComponentTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM MissionNPCComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM MissionNPCComponent");
	while (!tableData.eof()) {
		CDMissionNPCComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.missionID = tableData.getIntField("missionID", -1);
		entry.offersMission = tableData.getIntField("offersMission", -1) == 1 ? true : false;
		entry.acceptsMission = tableData.getIntField("acceptsMission", -1) == 1 ? true : false;
		entry.gate_version = tableData.getStringField("gate_version", "");

		entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Queries the table with a custom "where" clause
std::vector<CDMissionNPCComponent> CDMissionNPCComponentTable::Query(std::function<bool(CDMissionNPCComponent)> predicate) {

	std::vector<CDMissionNPCComponent> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}
