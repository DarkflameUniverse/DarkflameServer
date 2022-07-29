#include "CDLootTableTable.h"

//! Constructor
CDLootTableTable::CDLootTableTable(void) {

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
		entry.id = tableData.getIntField(0, -1);
		entry.itemid = tableData.getIntField(0, -1);
		entry.LootTableIndex = tableData.getIntField(1, -1);
		entry.id = tableData.getIntField(2, -1);
		entry.MissionDrop = tableData.getIntField(3, -1) == 1 ? true : false;
		entry.sortPriority = tableData.getIntField(4, -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDLootTableTable::~CDLootTableTable(void) {}

//! Returns the table's name
std::string CDLootTableTable::GetName(void) const {
	return "LootTable";
}

//! Queries the table with a custom "where" clause
std::vector<CDLootTable> CDLootTableTable::Query(std::function<bool(CDLootTable)> predicate) {

	std::vector<CDLootTable> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
const std::vector<CDLootTable>& CDLootTableTable::GetEntries(void) const {
	return this->entries;
}
