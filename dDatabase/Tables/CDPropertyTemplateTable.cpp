#include "CDPropertyTemplateTable.h"

CDPropertyTemplateTable::CDPropertyTemplateTable() {

	// First, get the size of the table
	size_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM PropertyTemplate;");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);
		tableSize.nextRow();
	}

	tableSize.finalize();

	this->entries.reserve(size);

	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PropertyTemplate;");
	while (!tableData.eof()) {
		auto entry = CDPropertyTemplate{
				static_cast<uint32_t>(tableData.getIntField("id", -1)),
				static_cast<uint32_t>(tableData.getIntField("mapID", -1)),
				static_cast<uint32_t>(tableData.getIntField("vendorMapID", -1)),
				tableData.getStringField("spawnName", "")
		};

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

CDPropertyTemplate CDPropertyTemplateTable::GetByMapID(uint32_t mapID) {
	for (const auto& entry : entries) {
		if (entry.mapID == mapID)
			return entry;
	}

	return defaultEntry;
}

