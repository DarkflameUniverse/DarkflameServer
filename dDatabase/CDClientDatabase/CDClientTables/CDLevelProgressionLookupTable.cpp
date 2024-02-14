#include "CDLevelProgressionLookupTable.h"

void CDLevelProgressionLookupTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM LevelProgressionLookup");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM LevelProgressionLookup");
	while (!tableData.eof()) {
		CDLevelProgressionLookup entry;
		entry.id = tableData.getIntField("id", -1);
		entry.requiredUScore = tableData.getIntField("requiredUScore", -1);
		entry.BehaviorEffect = tableData.getStringField("BehaviorEffect", "");

		entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDLevelProgressionLookup> CDLevelProgressionLookupTable::Query(std::function<bool(CDLevelProgressionLookup)> predicate) {

	std::vector<CDLevelProgressionLookup> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}
