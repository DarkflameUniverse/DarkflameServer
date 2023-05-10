#include "CDLevelProgressionLookupTable.h"

//! Constructor
CDLevelProgressionLookupTable::CDLevelProgressionLookupTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM LevelProgressionLookup");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM LevelProgressionLookup");
	while (!tableData.eof()) {
		CDLevelProgressionLookup entry;
		entry.id = tableData.getIntField("id", -1);
		entry.requiredUScore = tableData.getIntField("requiredUScore", -1);
		entry.BehaviorEffect = tableData.getStringField("BehaviorEffect", "");

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Queries the table with a custom "where" clause
std::vector<CDLevelProgressionLookup> CDLevelProgressionLookupTable::Query(std::function<bool(CDLevelProgressionLookup)> predicate) {

	std::vector<CDLevelProgressionLookup> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDLevelProgressionLookup> CDLevelProgressionLookupTable::GetEntries(void) const {
	return this->entries;
}

