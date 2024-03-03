#include "CDEmoteTable.h"

void CDEmoteTableTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Emotes");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		CDEmoteTable entry;
		entry.ID = tableData.getIntField("id", -1);
		entry.animationName = tableData.getStringField("animationName", "");
		entry.iconFilename = tableData.getStringField("iconFilename", "");
		entry.channel = tableData.getIntField("channel", -1);
		entry.locked = tableData.getIntField("locked", -1) != 0;
		entry.localize = tableData.getIntField("localize", -1) != 0;
		entry.locState = tableData.getIntField("locStatus", -1);
		entry.gateVersion = tableData.getStringField("gate_version", "");

		entries.insert(std::make_pair(entry.ID, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

CDEmoteTable* CDEmoteTableTable::GetEmote(int32_t id) {
	auto& entries = GetEntriesMutable();
	auto itr = entries.find(id);
	return itr != entries.end() ? &itr->second : nullptr;
}
