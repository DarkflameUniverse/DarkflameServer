#include "CDLootTableTable.h"

void CDLootTableTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM LootTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM LootTable");
	while (!tableData.eof()) {
		CDLootTable entry;
		entry.id = tableData.getIntField("id", -1);
		entry.itemid = tableData.getIntField("itemid", -1);
		entry.LootTableIndex = tableData.getIntField("LootTableIndex", -1);
		entry.id = tableData.getIntField("id", -1);
		entry.MissionDrop = tableData.getIntField("MissionDrop", -1) == 1 ? true : false;
		entry.sortPriority = tableData.getIntField("sortPriority", -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Queries the table with a custom "where" clause
std::vector<CDLootTable> CDLootTableTable::Query(std::function<bool(CDLootTable)> predicate) {

	std::vector<CDLootTable> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
const std::vector<CDLootTable>& CDLootTableTable::GetEntries() const {
	return this->entries;
}

