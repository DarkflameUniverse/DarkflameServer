#include "CDMissionEmailTable.h"

//! Constructor
CDMissionEmailTable::CDMissionEmailTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM MissionEmail");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM MissionEmail");
	while (!tableData.eof()) {
		CDMissionEmail entry;
		entry.ID = tableData.getIntField(0, -1);
		entry.messageType = tableData.getIntField(1, -1);
		entry.notificationGroup = tableData.getIntField(2, -1);
		entry.missionID = tableData.getIntField(3, -1);
		entry.attachmentLOT = tableData.getIntField(4, 0);
		entry.localize = (bool)tableData.getIntField(5, -1);
		entry.locStatus = tableData.getIntField(6, -1);
		entry.gate_version = tableData.getStringField(7, "");

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDMissionEmailTable::~CDMissionEmailTable(void) {}

//! Returns the table's name
std::string CDMissionEmailTable::GetName(void) const {
	return "MissionEmail";
}

//! Queries the table with a custom "where" clause
std::vector<CDMissionEmail> CDMissionEmailTable::Query(std::function<bool(CDMissionEmail)> predicate) {

	std::vector<CDMissionEmail> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDMissionEmail> CDMissionEmailTable::GetEntries(void) const {
	return this->entries;
}
