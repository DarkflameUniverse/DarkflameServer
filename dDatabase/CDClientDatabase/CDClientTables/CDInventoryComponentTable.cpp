#include "CDInventoryComponentTable.h"

void CDInventoryComponentTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM InventoryComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM InventoryComponent");
	while (!tableData.eof()) {
		CDInventoryComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.itemid = tableData.getIntField("itemid", -1);
		entry.count = tableData.getIntField("count", -1);
		entry.equip = tableData.getIntField("equip", -1) == 1 ? true : false;

		entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDInventoryComponent> CDInventoryComponentTable::Query(std::function<bool(CDInventoryComponent)> predicate) {
	std::vector<CDInventoryComponent> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}
