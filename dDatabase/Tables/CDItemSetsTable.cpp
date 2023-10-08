#include "CDItemSetsTable.h"

void CDItemSetsTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ItemSets");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ItemSets");
	while (!tableData.eof()) {
		CDItemSets entry;
		entry.setID = tableData.getIntField("setID", -1);
		entry.locStatus = tableData.getIntField("locStatus", -1);
		entry.itemIDs = tableData.getStringField("itemIDs", "");
		entry.kitType = tableData.getIntField("kitType", -1);
		entry.kitRank = tableData.getIntField("kitRank", -1);
		entry.kitImage = tableData.getIntField("kitImage", -1);
		entry.skillSetWith2 = tableData.getIntField("skillSetWith2", -1);
		entry.skillSetWith3 = tableData.getIntField("skillSetWith3", -1);
		entry.skillSetWith4 = tableData.getIntField("skillSetWith4", -1);
		entry.skillSetWith5 = tableData.getIntField("skillSetWith5", -1);
		entry.skillSetWith6 = tableData.getIntField("skillSetWith6", -1);
		entry.localize = tableData.getIntField("localize", -1) == 1 ? true : false;
		entry.gate_version = tableData.getStringField("gate_version", "");
		entry.kitID = tableData.getIntField("kitID", -1);
		entry.priority = tableData.getFloatField("priority", -1.0f);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDItemSets> CDItemSetsTable::Query(std::function<bool(CDItemSets)> predicate) {

	std::vector<CDItemSets> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const std::vector<CDItemSets>& CDItemSetsTable::GetEntries() const {
	return this->entries;
}

