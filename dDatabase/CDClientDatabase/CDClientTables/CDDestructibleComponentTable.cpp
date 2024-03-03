#include "CDDestructibleComponentTable.h"

void CDDestructibleComponentTable::LoadValuesFromDatabase() {
	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM DestructibleComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM DestructibleComponent");
	while (!tableData.eof()) {
		CDDestructibleComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.faction = tableData.getIntField("faction", -1);
		entry.factionList = tableData.getStringField("factionList", "");
		entry.life = tableData.getIntField("life", -1);
		entry.imagination = tableData.getIntField("imagination", -1);
		entry.LootMatrixIndex = tableData.getIntField("LootMatrixIndex", -1);
		entry.CurrencyIndex = tableData.getIntField("CurrencyIndex", -1);
		entry.level = tableData.getIntField("level", -1);
		entry.armor = tableData.getFloatField("armor", -1.0f);
		entry.death_behavior = tableData.getIntField("death_behavior", -1);
		entry.isnpc = tableData.getIntField("isnpc", -1) == 1 ? true : false;
		entry.attack_priority = tableData.getIntField("attack_priority", -1);
		entry.isSmashable = tableData.getIntField("isSmashable", -1) == 1 ? true : false;
		entry.difficultyLevel = tableData.getIntField("difficultyLevel", -1);

		entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDDestructibleComponent> CDDestructibleComponentTable::Query(std::function<bool(CDDestructibleComponent)> predicate) {
	std::vector<CDDestructibleComponent> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}
