#include "CDScriptComponentTable.h"

void CDScriptComponentTable::LoadValuesFromDatabase() {
	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ScriptComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ScriptComponent");
	while (!tableData.eof()) {
		CDScriptComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.script_name = tableData.getStringField("script_name", "");
		entry.client_script_name = tableData.getStringField("client_script_name", "");

		this->entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

const std::optional<CDScriptComponent> CDScriptComponentTable::GetByID(unsigned int id) {
	std::map<unsigned int, CDScriptComponent>::iterator it = this->entries.find(id);
	return (it != this->entries.end()) ? std::make_optional<CDScriptComponent>(it->second) : std::nullopt;
}

