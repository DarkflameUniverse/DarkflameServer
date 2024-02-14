#include "CDLootTableTable.h"
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "eReplicaComponentType.h"

// Sort the tables by their rarity so the highest rarity items are first.
void SortTable(LootTableEntries& table) {
	auto* componentsRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	auto* itemComponentTable = CDClientManager::GetTable<CDItemComponentTable>();
	// We modify the table in place so the outer loop keeps track of what is sorted
	// and the inner loop finds the highest rarity item and swaps it with the current position
	// of the outer loop.
	for (auto oldItrOuter = table.begin(); oldItrOuter != table.end(); oldItrOuter++) {
		auto lootToInsert = oldItrOuter;
		// Its fine if this starts at 0, even if this doesnt match lootToInsert as the actual highest will
		// either be found and overwrite these values, or the original is somehow zero and is still the highest rarity.
		uint32_t highestLootRarity = 0;
		for (auto oldItrInner = oldItrOuter; oldItrInner != table.end(); oldItrInner++) {
			uint32_t itemComponentId = componentsRegistryTable->GetByIDAndType(oldItrInner->itemid, eReplicaComponentType::ITEM);
			uint32_t rarity = itemComponentTable->GetItemComponentByID(itemComponentId).rarity;
			if (rarity > highestLootRarity) {
				highestLootRarity = rarity;
				lootToInsert = oldItrInner;
			}
		}
		std::swap(*oldItrOuter, *lootToInsert);
	}
}

CDLootTable CDLootTableTable::ReadRow(CppSQLite3Query& tableData) const {
	CDLootTable entry{};
	if (tableData.eof()) return entry;
	entry.itemid = tableData.getIntField("itemid", -1);
	entry.MissionDrop = tableData.getIntField("MissionDrop", -1) == 1 ? true : false;
	entry.sortPriority = tableData.getIntField("sortPriority", -1);
	return entry;
}

void CDLootTableTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM LootTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM LootTable");
	while (!tableData.eof()) {
		CDLootTable entry;
		uint32_t lootTableIndex = tableData.getIntField("LootTableIndex", -1);

		entries[lootTableIndex].push_back(ReadRow(tableData));
		tableData.nextRow();
	}
	for (auto& [id, table] : entries) {
		SortTable(table);
	}
}

const LootTableEntries& CDLootTableTable::GetTable(uint32_t tableId) {
	auto& entries = GetEntriesMutable();
	auto itr = entries.find(tableId);
	if (itr != entries.end()) {
		return itr->second;
	}

	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM LootTable WHERE LootTableIndex = ?;");
	query.bind(1, static_cast<int32_t>(tableId));
	auto tableData = query.execQuery();

	while (!tableData.eof()) {
		CDLootTable entry;
		entries[tableId].push_back(ReadRow(tableData));
		tableData.nextRow();
	}
	SortTable(entries[tableId]);

	return entries[tableId];
}
