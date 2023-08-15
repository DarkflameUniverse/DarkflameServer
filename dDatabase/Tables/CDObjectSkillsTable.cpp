#include "CDObjectSkillsTable.h"

//! Constructor
CDObjectSkillsTable::CDObjectSkillsTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ObjectSkills");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ObjectSkills");
	while (!tableData.eof()) {
		CDObjectSkills entry;
		entry.objectTemplate = tableData.getIntField("objectTemplate", -1);
		entry.skillID = tableData.getIntField("skillID", -1);
		entry.castOnType = tableData.getIntField("castOnType", -1);
		entry.AICombatWeight = tableData.getIntField("AICombatWeight", -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Queries the table with a custom "where" clause
std::vector<CDObjectSkills> CDObjectSkillsTable::Query(std::function<bool(CDObjectSkills)> predicate) {

	std::vector<CDObjectSkills> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDObjectSkills> CDObjectSkillsTable::GetEntries(void) const {
	return this->entries;
}
