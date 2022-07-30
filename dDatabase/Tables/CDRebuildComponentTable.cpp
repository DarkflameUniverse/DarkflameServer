#include "CDRebuildComponentTable.h"

//! Constructor
CDRebuildComponentTable::CDRebuildComponentTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM RebuildComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM RebuildComponent");
	while (!tableData.eof()) {
		CDRebuildComponent entry;
		entry.id = tableData.getIntField(0, -1);
		entry.reset_time = tableData.getFloatField(1, -1.0f);
		entry.complete_time = tableData.getFloatField(2, -1.0f);
		entry.take_imagination = tableData.getIntField(3, -1);
		entry.interruptible = tableData.getIntField(4, -1) == 1 ? true : false;
		entry.self_activator = tableData.getIntField(5, -1) == 1 ? true : false;
		entry.custom_modules = tableData.getStringField(6, "");
		entry.activityID = tableData.getIntField(7, -1);
		entry.post_imagination_cost = tableData.getIntField(8, -1);
		entry.time_before_smash = tableData.getFloatField(9, -1.0f);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDRebuildComponentTable::~CDRebuildComponentTable(void) {}

//! Returns the table's name
std::string CDRebuildComponentTable::GetName(void) const {
	return "RebuildComponent";
}

//! Queries the table with a custom "where" clause
std::vector<CDRebuildComponent> CDRebuildComponentTable::Query(std::function<bool(CDRebuildComponent)> predicate) {

	std::vector<CDRebuildComponent> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDRebuildComponent> CDRebuildComponentTable::GetEntries(void) const {
	return this->entries;
}
