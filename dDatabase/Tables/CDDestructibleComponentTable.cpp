#include "CDDestructibleComponentTable.h"

//! Constructor
CDDestructibleComponentTable::CDDestructibleComponentTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM DestructibleComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM DestructibleComponent");
	while (!tableData.eof()) {
		CDDestructibleComponent entry;
		entry.id = tableData.getIntField(0, -1);
		entry.faction = tableData.getIntField(1, -1);
		entry.factionList = tableData.getStringField(2, "");
		entry.life = tableData.getIntField(3, -1);
		entry.imagination = tableData.getIntField(4, -1);
		entry.LootMatrixIndex = tableData.getIntField(5, -1);
		entry.CurrencyIndex = tableData.getIntField(6, -1);
		entry.level = tableData.getIntField(7, -1);
		entry.armor = tableData.getFloatField(8, -1.0f);
		entry.death_behavior = tableData.getIntField(9, -1);
		entry.isnpc = tableData.getIntField(10, -1) == 1 ? true : false;
		entry.attack_priority = tableData.getIntField(11, -1);
		entry.isSmashable = tableData.getIntField(12, -1) == 1 ? true : false;
		entry.difficultyLevel = tableData.getIntField(13, -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDDestructibleComponentTable::~CDDestructibleComponentTable(void) {}

//! Returns the table's name
std::string CDDestructibleComponentTable::GetName(void) const {
	return "DestructibleComponent";
}

//! Queries the table with a custom "where" clause
std::vector<CDDestructibleComponent> CDDestructibleComponentTable::Query(std::function<bool(CDDestructibleComponent)> predicate) {

	std::vector<CDDestructibleComponent> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDDestructibleComponent> CDDestructibleComponentTable::GetEntries(void) const {
	return this->entries;
}
