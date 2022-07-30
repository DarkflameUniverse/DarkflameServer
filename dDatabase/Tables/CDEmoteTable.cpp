#include "CDEmoteTable.h"

//! Constructor
CDEmoteTableTable::CDEmoteTableTable(void) {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Emotes");
	while (!tableData.eof()) {
		CDEmoteTable* entry = new CDEmoteTable();
		entry->ID = tableData.getIntField(0, -1);
		entry->animationName = tableData.getStringField(1, "");
		entry->iconFilename = tableData.getStringField(2, "");
		entry->channel = tableData.getIntField(3, -1);
		entry->locked = tableData.getIntField(5, -1) != 0;
		entry->localize = tableData.getIntField(6, -1) != 0;
		entry->locState = tableData.getIntField(7, -1);
		entry->gateVersion = tableData.getIntField(8, -1);

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
