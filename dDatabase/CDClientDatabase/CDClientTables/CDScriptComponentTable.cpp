#include "CDScriptComponentTable.h"

namespace {
	CDScriptComponent m_ToReturnWhenNoneFound;
};

void CDScriptComponentTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ScriptComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ScriptComponent");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		CDScriptComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.script_name = tableData.getStringField("script_name", "");
		entry.client_script_name = tableData.getStringField("client_script_name", "");

		entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

const CDScriptComponent& CDScriptComponentTable::GetByID(uint32_t id) {
	auto& entries = GetEntries();
	auto it = entries.find(id);
	if (it != entries.end()) {
		return it->second;
	}

	return m_ToReturnWhenNoneFound;
}

