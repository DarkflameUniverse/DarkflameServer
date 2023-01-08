#include "CDEmoteTable.h"

//! Constructor
CDEmoteTableTable::CDEmoteTableTable(void) {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Emotes");
	while (!tableData.eof()) {
		CDEmoteTable* entry = new CDEmoteTable();
		entry->ID = tableData.getIntField("id", -1);
		entry->animationName = tableData.getStringField("animationName", "");
		entry->iconFilename = tableData.getStringField("iconFilename", "");
		entry->channel = tableData.getIntField("channel", -1);
		entry->locked = tableData.getIntField("locked", -1) != 0;
		entry->localize = tableData.getIntField("localize", -1) != 0;
		entry->locState = tableData.getIntField("locStatus", -1);
		entry->gateVersion = tableData.getStringField("gate_version", "");

		entries.insert(std::make_pair(entry->ID, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDEmoteTableTable::~CDEmoteTableTable(void) {
	for (auto e : entries) {
		if (e.second) delete e.second;
	}

	entries.clear();
}

//! Returns the table's name
std::string CDEmoteTableTable::GetName(void) const {
	return "Emotes";
}

CDEmoteTable* CDEmoteTableTable::GetEmote(int id) {
	for (auto e : entries) {
		if (e.first == id) return e.second;
	}

	return nullptr;
}

