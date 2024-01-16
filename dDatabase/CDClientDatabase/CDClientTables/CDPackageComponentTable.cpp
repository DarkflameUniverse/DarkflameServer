#include "CDPackageComponentTable.h"

void CDPackageComponentTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM PackageComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PackageComponent");
	while (!tableData.eof()) {
		CDPackageComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.LootMatrixIndex = tableData.getIntField("LootMatrixIndex", -1);
		entry.packageType = tableData.getIntField("packageType", -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Queries the table with a custom "where" clause
std::vector<CDPackageComponent> CDPackageComponentTable::Query(std::function<bool(CDPackageComponent)> predicate) {

	std::vector<CDPackageComponent> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
const std::vector<CDPackageComponent>& CDPackageComponentTable::GetEntries() const {
	return this->entries;
}

