#include "CDProximityMonitorComponentTable.h"

void CDProximityMonitorComponentTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ProximityMonitorComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ProximityMonitorComponent");
	while (!tableData.eof()) {
		CDProximityMonitorComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.Proximities = tableData.getStringField("Proximities", "");
		entry.LoadOnClient = tableData.getIntField("LoadOnClient", -1);
		entry.LoadOnServer = tableData.getIntField("LoadOnServer", -1);

		entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDProximityMonitorComponent> CDProximityMonitorComponentTable::Query(std::function<bool(CDProximityMonitorComponent)> predicate) {

	std::vector<CDProximityMonitorComponent> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}
