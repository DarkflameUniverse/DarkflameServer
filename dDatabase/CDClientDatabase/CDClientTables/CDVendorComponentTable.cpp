#include "CDVendorComponentTable.h"

void CDVendorComponentTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM VendorComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM VendorComponent");
	while (!tableData.eof()) {
		CDVendorComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.buyScalar = tableData.getFloatField("buyScalar", 0.0f);
		entry.sellScalar = tableData.getFloatField("sellScalar", -1.0f);
		entry.refreshTimeSeconds = tableData.getFloatField("refreshTimeSeconds", -1.0f);
		entry.LootMatrixIndex = tableData.getIntField("LootMatrixIndex", -1);

		entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Queries the table with a custom "where" clause
std::vector<CDVendorComponent> CDVendorComponentTable::Query(std::function<bool(CDVendorComponent)> predicate) {

	std::vector<CDVendorComponent> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}
