#include "CDMissionTasksTable.h"

void CDMissionTasksTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM MissionTasks");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM MissionTasks");
	while (!tableData.eof()) {
		auto& entry = entries.emplace_back();
		entry.id = tableData.getIntField("id", -1);
		UNUSED(entry.locStatus = tableData.getIntField("locStatus", -1));
		entry.taskType = tableData.getIntField("taskType", -1);
		entry.target = tableData.getIntField("target", -1);
		entry.targetGroup = tableData.getStringField("targetGroup", "");
		entry.targetValue = tableData.getIntField("targetValue", -1);
		entry.taskParam1 = tableData.getStringField("taskParam1", "");
		UNUSED(entry.largeTaskIcon = tableData.getStringField("largeTaskIcon", ""));
		UNUSED(entry.IconID = tableData.getIntField("IconID", -1));
		entry.uid = tableData.getIntField("uid", -1);
		UNUSED(entry.largeTaskIconID = tableData.getIntField("largeTaskIconID", -1));
		UNUSED(entry.localize = tableData.getIntField("localize", -1) == 1 ? true : false);
		UNUSED(entry.gate_version = tableData.getStringField("gate_version", ""));

		tableData.nextRow();
	}
}

std::vector<CDMissionTasks> CDMissionTasksTable::Query(std::function<bool(CDMissionTasks)> predicate) {

	std::vector<CDMissionTasks> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

std::vector<CDMissionTasks*> CDMissionTasksTable::GetByMissionID(const uint32_t missionID) {
	std::vector<CDMissionTasks*> tasks;

	// TODO: this should not be linear(?) and also shouldnt need to be a pointer
	for (auto& entry : GetEntriesMutable()) {
		if (entry.id == missionID) {
			tasks.push_back(&entry);
		}
	}

	return tasks;
}

const typename CDMissionTasksTable::StorageType& CDMissionTasksTable::GetEntries() const {
	return CDTable::GetEntries();
}
