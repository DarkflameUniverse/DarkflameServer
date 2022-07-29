#include "CDScriptComponentTable.h"

//! Constructor
CDScriptComponentTable::CDScriptComponentTable(void) {

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
		entry.id = tableData.getIntField(0, -1);
		entry.script_name = tableData.getStringField(1, "");
		entry.client_script_name = tableData.getStringField(2, "");

		this->entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDScriptComponentTable::~CDScriptComponentTable(void) {}

//! Returns the table's name
std::string CDScriptComponentTable::GetName(void) const {
	return "ScriptComponent";
}

const CDScriptComponent& CDScriptComponentTable::GetByID(unsigned int id) {
	std::map<unsigned int, CDScriptComponent>::iterator it = this->entries.find(id);
	if (it != this->entries.end()) {
		return it->second;
	}

	return m_ToReturnWhenNoneFound;
}
