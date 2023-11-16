#include "CDRarityTableTable.h"

void CDRarityTableTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM RarityTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM RarityTable order by randmax desc;");
	while (!tableData.eof()) {
		uint32_t rarityTableIndex = tableData.getIntField("RarityTableIndex", -1);

		CDRarityTable entry;
		entry.randmax = tableData.getFloatField("randmax", -1);
		entry.rarity = tableData.getIntField("rarity", -1);
		entries[rarityTableIndex].push_back(entry);
		tableData.nextRow();
	}
}

const std::vector<CDRarityTable>& CDRarityTableTable::GetRarityTable(uint32_t id) {
	return entries[id];
}
