#include "CDScriptComponentTable.h"

void CDScriptComponentTable::LoadValuesFromDatabase() {
	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ScriptComponent");
	while (!tableData.eof()) {
		CDScriptComponent entry;
		uint32_t id = tableData.getIntField("id", -1);
		entry.script_name = tableData.getStringField("script_name", "");
		entry.client_script_name = tableData.getStringField("client_script_name", "");

		this->entries.insert_or_assign(id, entry);
		tableData.nextRow();
	}
}

const std::optional<CDScriptComponent> CDScriptComponentTable::GetByID(unsigned int id) {
	auto it = this->entries.find(id);
	return (it != this->entries.end()) ? std::make_optional<CDScriptComponent>(it->second) : std::nullopt;
}

