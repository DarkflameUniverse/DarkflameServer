#include "CDLootTableTable.h"

void CDLootTableTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM LootTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM LootTable");
	while (!tableData.eof()) {
		CDLootTable entry;
		uint32_t lootTableIndex = tableData.getIntField("LootTableIndex", -1);
		entry.itemid = tableData.getIntField("itemid", -1);
		entry.MissionDrop = tableData.getIntField("MissionDrop", -1) == 1 ? true : false;
		entry.sortPriority = tableData.getIntField("sortPriority", -1);

		this->entries[lootTableIndex].push_back(entry);
		tableData.nextRow();
	}
}

const LootTableEntries& CDLootTableTable::GetTable(uint32_t tableId) {
	auto itr = this->entries.find(tableId);
	if (itr != this->entries.end()) {
		return itr->second;
	}
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM LootTable WHERE LootTableIndex = ?;");
	query.bind(1, static_cast<int32_t>(tableId));

	auto tableData = query.execQuery();
	while (!tableData.eof()) {
		CDLootTable entry;
		entry.itemid = tableData.getIntField("itemid", -1);
		entry.MissionDrop = tableData.getIntField("MissionDrop", -1) == 1 ? true : false;
		entry.sortPriority = tableData.getIntField("sortPriority", -1);

		this->entries[tableId].push_back(entry);
		tableData.nextRow();
	}
	return this->entries[tableId];
}
