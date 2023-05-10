#include "CDRarityTableTable.h"

//! Constructor
CDRarityTableTable::CDRarityTableTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM RarityTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM RarityTable");
	while (!tableData.eof()) {
		CDRarityTable entry;
		entry.id = tableData.getIntField("id", -1);
		entry.randmax = tableData.getFloatField("randmax", -1);
		entry.rarity = tableData.getIntField("rarity", -1);
		entry.RarityTableIndex = tableData.getIntField("RarityTableIndex", -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Queries the table with a custom "where" clause
std::vector<CDRarityTable> CDRarityTableTable::Query(std::function<bool(CDRarityTable)> predicate) {

	std::vector<CDRarityTable> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
const std::vector<CDRarityTable>& CDRarityTableTable::GetEntries(void) const {
	return this->entries;
}

