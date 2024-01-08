#include "CDObjectSkillsTable.h"

void CDObjectSkillsTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ObjectSkills");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	m_Entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ObjectSkills");
	while (!tableData.eof()) {
		auto &entry = m_Entries.emplace_back();
		entry.objectTemplate = tableData.getIntField("objectTemplate", -1);
		entry.skillID = tableData.getIntField("skillID", -1);
		entry.castOnType = tableData.getIntField("castOnType", -1);
		entry.AICombatWeight = tableData.getIntField("AICombatWeight", -1);

		tableData.nextRow();
	}
}

std::vector<CDObjectSkills> CDObjectSkillsTable::Query(std::function<bool(CDObjectSkills)> predicate) {

	std::vector<CDObjectSkills> data = cpplinq::from(m_Entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const std::vector<CDObjectSkills>& CDObjectSkillsTable::GetEntries() const {
	return m_Entries;
}
