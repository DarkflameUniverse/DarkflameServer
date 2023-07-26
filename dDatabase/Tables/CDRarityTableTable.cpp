#include "CDRarityTableTable.h"

void CDRarityTableTable::LoadValuesFromDatabase() {
	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM RarityTable");
	while (!tableData.eof()) {
		CDRarityTable entry;
		uint32_t id = tableData.getIntField("id", -1);
		entry.randmax = tableData.getFloatField("randmax", -1);
		entry.rarity = tableData.getIntField("rarity", -1);
		entry.RarityTableIndex = tableData.getIntField("RarityTableIndex", -1);

		this->entries.insert_or_assign(id, entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Queries the table with a custom "where" clause
const std::optional<CDRarityTable> CDRarityTableTable::Get(uint32_t id) {
	auto it = this->entries.find(id);
	return it != this->entries.end() ? std::make_optional(it->second) : std::nullopt;
}
