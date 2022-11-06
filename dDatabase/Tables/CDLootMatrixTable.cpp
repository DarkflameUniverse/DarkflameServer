#include "CDLootMatrixTable.h"

//! Constructor
CDLootMatrixTable::CDLootMatrixTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM LootMatrix");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM LootMatrix");
	while (!tableData.eof()) {
		CDLootMatrix entry;
		entry.LootMatrixIndex = tableData.getIntField(0, -1);
		entry.LootTableIndex = tableData.getIntField(1, -1);
		entry.RarityTableIndex = tableData.getIntField(2, -1);
		entry.percent = tableData.getFloatField(3, -1.0f);
		entry.minToDrop = tableData.getIntField(4, -1);
		entry.maxToDrop = tableData.getIntField(5, -1);
		entry.id = tableData.getIntField(6, -1);
		entry.flagID = tableData.getIntField(7, -1);
		UNUSED(entry.gate_version = tableData.getStringField(8, ""));

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDLootMatrixTable::~CDLootMatrixTable(void) {}

//! Returns the table's name
std::string CDLootMatrixTable::GetName(void) const {
	return "LootMatrix";
}

//! Queries the table with a custom "where" clause
std::vector<CDLootMatrix> CDLootMatrixTable::Query(std::function<bool(CDLootMatrix)> predicate) {

	std::vector<CDLootMatrix> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
const std::vector<CDLootMatrix>& CDLootMatrixTable::GetEntries(void) const {
	return this->entries;
}
