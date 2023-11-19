#include "CDPropertyEntranceComponentTable.h"

void CDPropertyEntranceComponentTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	size_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM PropertyEntranceComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);
		tableSize.nextRow();
	}

	tableSize.finalize();

	this->entries.reserve(size);

	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PropertyEntranceComponent;");
	while (!tableData.eof()) {
		auto entry = CDPropertyEntranceComponent{
			static_cast<uint32_t>(tableData.getIntField("id", -1)),
			static_cast<uint32_t>(tableData.getIntField("mapID", -1)),
			tableData.getStringField("propertyName", ""),
			static_cast<bool>(tableData.getIntField("isOnProperty", false)),
			tableData.getStringField("groupType", "")
		};

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

CDPropertyEntranceComponent CDPropertyEntranceComponentTable::GetByID(uint32_t id) {
	for (const auto& entry : entries) {
		if (entry.id == id)
			return entry;
	}

	return defaultEntry;
}

