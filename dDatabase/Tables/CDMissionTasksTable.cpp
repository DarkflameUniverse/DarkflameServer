#include "CDMissionTasksTable.h"

//! Constructor
CDMissionTasksTable::CDMissionTasksTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM MissionTasks");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM MissionTasks");
	while (!tableData.eof()) {
		CDMissionTasks entry;
		entry.id = tableData.getIntField(0, -1);
		UNUSED(entry.locStatus = tableData.getIntField(1, -1));
		entry.taskType = tableData.getIntField(2, -1);
		entry.target = tableData.getIntField(3, -1);
		entry.targetGroup = tableData.getStringField(4, "");
		entry.targetValue = tableData.getIntField(5, -1);
		entry.taskParam1 = tableData.getStringField(6, "");
		UNUSED(entry.largeTaskIcon = tableData.getStringField(7, ""));
		UNUSED(entry.IconID = tableData.getIntField(8, -1));
		entry.uid = tableData.getIntField(9, -1);
		UNUSED(entry.largeTaskIconID = tableData.getIntField(10, -1));
		UNUSED(entry.localize = tableData.getIntField(11, -1) == 1 ? true : false);
		UNUSED(entry.gate_version = tableData.getStringField(12, ""));

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDMissionTasksTable::~CDMissionTasksTable(void) {}

//! Returns the table's name
std::string CDMissionTasksTable::GetName(void) const {
	return "MissionTasks";
}

//! Queries the table with a custom "where" clause
std::vector<CDMissionTasks> CDMissionTasksTable::Query(std::function<bool(CDMissionTasks)> predicate) {

	std::vector<CDMissionTasks> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

std::vector<CDMissionTasks*> CDMissionTasksTable::GetByMissionID(uint32_t missionID) {
	std::vector<CDMissionTasks*> tasks;

	for (auto& entry : this->entries) {
		if (entry.id == missionID) {
			CDMissionTasks* task = const_cast<CDMissionTasks*>(&entry);

			tasks.push_back(task);
		}
	}

	return tasks;
}

//! Gets all the entries in the table
const std::vector<CDMissionTasks>& CDMissionTasksTable::GetEntries(void) const {
	return this->entries;
}
