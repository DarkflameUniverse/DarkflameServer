#include "CDItemSetSkillsTable.h"

void CDItemSetSkillsTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
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
		entry.SkillSetID = tableData.getIntField("SkillSetID", -1);
		entry.SkillID = tableData.getIntField("SkillID", -1);
		entry.SkillCastType = tableData.getIntField("SkillCastType", -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDItemSetSkills> CDItemSetSkillsTable::Query(std::function<bool(CDItemSetSkills)> predicate) {

	std::vector<CDItemSetSkills> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const std::vector<CDItemSetSkills>& CDItemSetSkillsTable::GetEntries() const {
	return this->entries;
}

std::vector<CDItemSetSkills> CDItemSetSkillsTable::GetBySkillID(uint32_t SkillSetID) {
	std::vector<CDItemSetSkills> toReturn;

	for (CDItemSetSkills entry : this->entries) {
		if (entry.SkillSetID == SkillSetID) toReturn.push_back(entry);
		if (entry.SkillSetID > SkillSetID) return toReturn; //stop seeking in the db if it's not needed.
	}

	return toReturn;
}
