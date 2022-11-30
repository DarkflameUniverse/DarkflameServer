#include "CDItemSetSkillsTable.h"

//! Constructor
CDItemSetSkillsTable::CDItemSetSkillsTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ItemSetSkills");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ItemSetSkills");
	while (!tableData.eof()) {
		CDItemSetSkills entry;
		entry.SkillSetID = tableData.getIntField(0, -1);
		entry.SkillID = tableData.getIntField(1, -1);
		entry.SkillCastType = tableData.getIntField(2, -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDItemSetSkillsTable::~CDItemSetSkillsTable(void) {}

//! Returns the table's name
std::string CDItemSetSkillsTable::GetName(void) const {
	return "ItemSetSkills";
}

//! Queries the table with a custom "where" clause
std::vector<CDItemSetSkills> CDItemSetSkillsTable::Query(std::function<bool(CDItemSetSkills)> predicate) {

	std::vector<CDItemSetSkills> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDItemSetSkills> CDItemSetSkillsTable::GetEntries(void) const {
	return this->entries;
}

std::vector<CDItemSetSkills> CDItemSetSkillsTable::GetBySkillID(unsigned int SkillSetID) {
	std::vector<CDItemSetSkills> toReturn;

	for (CDItemSetSkills entry : this->entries) {
		if (entry.SkillSetID == SkillSetID) toReturn.push_back(entry);
		if (entry.SkillSetID > SkillSetID) return toReturn; //stop seeking in the db if it's not needed.
	}

	return toReturn;
}
