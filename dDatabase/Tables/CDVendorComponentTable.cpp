#include "CDVendorComponentTable.h"

//! Constructor
CDVendorComponentTable::CDVendorComponentTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM VendorComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM VendorComponent");
	while (!tableData.eof()) {
		CDVendorComponent entry;
		entry.id = tableData.getIntField(0, -1);
		entry.buyScalar = tableData.getFloatField(1, -1.0f);
		entry.sellScalar = tableData.getFloatField(2, -1.0f);
		entry.refreshTimeSeconds = tableData.getFloatField(3, -1.0f);
		entry.LootMatrixIndex = tableData.getIntField(4, -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDVendorComponentTable::~CDVendorComponentTable(void) {}

//! Returns the table's name
std::string CDVendorComponentTable::GetName(void) const {
	return "VendorComponent";
}

//! Queries the table with a custom "where" clause
std::vector<CDVendorComponent> CDVendorComponentTable::Query(std::function<bool(CDVendorComponent)> predicate) {

	std::vector<CDVendorComponent> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDVendorComponent> CDVendorComponentTable::GetEntries(void) const {
	return this->entries;
}
