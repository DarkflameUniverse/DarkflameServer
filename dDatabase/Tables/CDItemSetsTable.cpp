#include "CDItemSetsTable.h"

//! Constructor
CDItemSetsTable::CDItemSetsTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ItemSets");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ItemSets");
	while (!tableData.eof()) {
		CDItemSets entry;
		entry.setID = tableData.getIntField(0, -1);
		entry.locStatus = tableData.getIntField(1, -1);
		entry.itemIDs = tableData.getStringField(2, "");
		entry.kitType = tableData.getIntField(3, -1);
		entry.kitRank = tableData.getIntField(4, -1);
		entry.kitImage = tableData.getIntField(5, -1);
		entry.skillSetWith2 = tableData.getIntField(6, -1);
		entry.skillSetWith3 = tableData.getIntField(7, -1);
		entry.skillSetWith4 = tableData.getIntField(8, -1);
		entry.skillSetWith5 = tableData.getIntField(9, -1);
		entry.skillSetWith6 = tableData.getIntField(10, -1);
		entry.localize = tableData.getIntField(11, -1) == 1 ? true : false;
		entry.gate_version = tableData.getStringField(12, "");
		entry.kitID = tableData.getIntField(13, -1);
		entry.priority = tableData.getFloatField(14, -1.0f);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDItemSetsTable::~CDItemSetsTable(void) {}

//! Returns the table's name
std::string CDItemSetsTable::GetName(void) const {
	return "ItemSets";
}

//! Queries the table with a custom "where" clause
std::vector<CDItemSets> CDItemSetsTable::Query(std::function<bool(CDItemSets)> predicate) {

	std::vector<CDItemSets> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDItemSets> CDItemSetsTable::GetEntries(void) const {
	return this->entries;
}
