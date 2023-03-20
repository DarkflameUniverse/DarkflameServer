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
		entry.id = tableData.getIntField("id", -1);
		UNUSED_COLUMN(entry.locStatus = tableData.getIntField("locStatus", -1));
		entry.taskType = tableData.getIntField("taskType", -1);
		entry.target = tableData.getIntField("target", -1);
		entry.targetGroup = tableData.getStringField("targetGroup", "");
		entry.targetValue = tableData.getIntField("targetValue", -1);
		entry.taskParam1 = tableData.getStringField("taskParam1", "");
		UNUSED_COLUMN(entry.largeTaskIcon = tableData.getStringField("largeTaskIcon", ""));
		UNUSED_COLUMN(entry.IconID = tableData.getIntField("IconID", -1));
		entry.uid = tableData.getIntField("uid", -1);
		UNUSED_COLUMN(entry.largeTaskIconID = tableData.getIntField("largeTaskIconID", -1));
		UNUSED_COLUMN(entry.localize = tableData.getIntField("localize", -1) == 1 ? true : false);
		UNUSED_COLUMN(entry.gate_version = tableData.getStringField("gate_version", ""));

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

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

const std::vector<CDMissionTasks>& CDMissionTasksTable::GetEntries(void) const {
	return this->entries;
}

