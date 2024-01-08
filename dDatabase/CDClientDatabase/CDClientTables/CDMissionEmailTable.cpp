#include "CDMissionEmailTable.h"

void CDMissionEmailTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM MissionEmail");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	m_Entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM MissionEmail");
	while (!tableData.eof()) {
		auto& entry = m_Entries.emplace_back();
		entry.ID = tableData.getIntField("ID", -1);
		entry.messageType = tableData.getIntField("messageType", -1);
		entry.notificationGroup = tableData.getIntField("notificationGroup", -1);
		entry.missionID = tableData.getIntField("missionID", -1);
		entry.attachmentLOT = tableData.getIntField("attachmentLOT", 0);
		entry.localize = static_cast<bool>(tableData.getIntField("localize", 1));
		entry.locStatus = tableData.getIntField("locStatus", -1);
		entry.gate_version = tableData.getStringField("gate_version", "");

		tableData.nextRow();
	}
}

//! Queries the table with a custom "where" clause
std::vector<CDMissionEmail> CDMissionEmailTable::Query(std::function<bool(CDMissionEmail)> predicate) {

	std::vector<CDMissionEmail> data = cpplinq::from(m_Entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
const std::vector<CDMissionEmail>& CDMissionEmailTable::GetEntries() const {
	return m_Entries;
}
