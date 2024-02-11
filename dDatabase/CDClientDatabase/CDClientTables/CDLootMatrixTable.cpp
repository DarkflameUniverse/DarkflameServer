#include "CDLootMatrixTable.h"

CDLootMatrix CDLootMatrixTable::ReadRow(CppSQLite3Query& tableData) const {
	CDLootMatrix entry{};
	if (tableData.eof()) return entry;
	entry.LootTableIndex = tableData.getIntField("LootTableIndex", -1);
	entry.RarityTableIndex = tableData.getIntField("RarityTableIndex", -1);
	entry.percent = tableData.getFloatField("percent", -1.0f);
	entry.minToDrop = tableData.getIntField("minToDrop", -1);
	entry.maxToDrop = tableData.getIntField("maxToDrop", -1);
	entry.flagID = tableData.getIntField("flagID", -1);
	UNUSED(entry.gate_version = tableData.getStringField("gate_version", ""));
	return entry;
}

void CDLootMatrixTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM LootMatrix");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM LootMatrix");
	while (!tableData.eof()) {
		CDLootMatrix entry;
		uint32_t lootMatrixIndex = tableData.getIntField("LootMatrixIndex", -1);

		entries[lootMatrixIndex].push_back(ReadRow(tableData));
		tableData.nextRow();
	}
}

const LootMatrixEntries& CDLootMatrixTable::GetMatrix(uint32_t matrixId) {
	auto& entries = GetEntriesMutable();
	auto itr = entries.find(matrixId);
	if (itr != entries.end()) {
		return itr->second;
	}

	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM LootMatrix where LootMatrixIndex = ?;");
	query.bind(1, static_cast<int32_t>(matrixId));

	auto tableData = query.execQuery();
	while (!tableData.eof()) {
		entries[matrixId].push_back(ReadRow(tableData));
		tableData.nextRow();
	}

	return entries[matrixId];
}

