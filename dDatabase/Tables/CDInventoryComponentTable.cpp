#include "CDInventoryComponentTable.h"

//! Constructor
CDInventoryComponentTable::CDInventoryComponentTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM InventoryComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM InventoryComponent");
	while (!tableData.eof()) {
		CDInventoryComponent entry;
		entry.id = tableData.getIntField(0, -1);
		entry.itemid = tableData.getIntField(1, -1);
		entry.count = tableData.getIntField(2, -1);
		entry.equip = tableData.getIntField(3, -1) == 1 ? true : false;

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDInventoryComponentTable::~CDInventoryComponentTable(void) {}

//! Returns the table's name
std::string CDInventoryComponentTable::GetName(void) const {
	return "InventoryComponent";
}

//! Queries the table with a custom "where" clause
std::vector<CDInventoryComponent> CDInventoryComponentTable::Query(std::function<bool(CDInventoryComponent)> predicate) {

	std::vector<CDInventoryComponent> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDInventoryComponent> CDInventoryComponentTable::GetEntries(void) const {
	return this->entries;
}
