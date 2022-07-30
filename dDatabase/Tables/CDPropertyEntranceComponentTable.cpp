
#include "CDPropertyEntranceComponentTable.h"

CDPropertyEntranceComponentTable::CDPropertyEntranceComponentTable() {

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
			static_cast<uint32_t>(tableData.getIntField(0, -1)),
			static_cast<uint32_t>(tableData.getIntField(1, -1)),
			tableData.getStringField(2, ""),
			static_cast<bool>(tableData.getIntField(3, false)),
			tableData.getStringField(4, "")
		};

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

CDPropertyEntranceComponentTable::~CDPropertyEntranceComponentTable(void) = default;

std::string CDPropertyEntranceComponentTable::GetName() const {
	return "PropertyEntranceComponent";
}

CDPropertyEntranceComponent CDPropertyEntranceComponentTable::GetByID(uint32_t id) {
	for (const auto& entry : entries) {
		if (entry.id == id)
			return entry;
	}

	return defaultEntry;
}
