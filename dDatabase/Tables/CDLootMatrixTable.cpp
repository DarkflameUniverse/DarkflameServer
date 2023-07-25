#include "CDLootMatrixTable.h"

void CDLootMatrixTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM LootMatrix");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM LootMatrix");
	while (!tableData.eof()) {
		CDLootMatrix entry;
		entry.LootMatrixIndex = tableData.getIntField("LootMatrixIndex", -1);
		entry.LootTableIndex = tableData.getIntField("LootTableIndex", -1);
		entry.RarityTableIndex = tableData.getIntField("RarityTableIndex", -1);
		entry.percent = tableData.getFloatField("percent", -1.0f);
		entry.minToDrop = tableData.getIntField("minToDrop", -1);
		entry.maxToDrop = tableData.getIntField("maxToDrop", -1);
		entry.id = tableData.getIntField("id", -1);
		entry.flagID = tableData.getIntField("flagID", -1);
		UNUSED(entry.gate_version = tableData.getStringField("gate_version", ""));

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDLootMatrix> CDLootMatrixTable::Query(std::function<bool(CDLootMatrix)> predicate) {

	std::vector<CDLootMatrix> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const std::vector<CDLootMatrix>& CDLootMatrixTable::GetEntries() const {
	return this->entries;
}

