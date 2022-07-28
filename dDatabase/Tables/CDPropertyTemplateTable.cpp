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
				static_cast<uint32_t>(tableData.getIntField(0, -1)),
				static_cast<uint32_t>(tableData.getIntField(1, -1)),
				static_cast<uint32_t>(tableData.getIntField(2, -1)),
				tableData.getStringField(3, "")
		};

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

CDPropertyTemplateTable::~CDPropertyTemplateTable() = default;

std::string CDPropertyTemplateTable::GetName() const {
	return "PropertyTemplate";
}

CDPropertyTemplate CDPropertyTemplateTable::GetByMapID(uint32_t mapID) {
	for (const auto& entry : entries) {
		if (entry.mapID == mapID)
			return entry;
	}

	return defaultEntry;
}
