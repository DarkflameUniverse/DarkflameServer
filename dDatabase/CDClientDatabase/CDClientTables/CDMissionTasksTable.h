#pragma once

// Custom Classes
#include "CDTable.h"

struct CDMissionTasks {
	uint32_t id;                //!< The Mission ID that the task belongs to
	UNUSED(uint32_t locStatus);         //!< ???
	uint32_t taskType;          //!< The task type
	uint32_t target;            //!< The mission target
	std::string targetGroup;        //!< The mission target group
	int32_t targetValue;       //!< The target value
	std::string taskParam1;    //!< The task param 1
	UNUSED(std::string largeTaskIcon); //!< ???
	UNUSED(uint32_t IconID);            //!< ???
	uint32_t uid;               //!< ???
	UNUSED(uint32_t largeTaskIconID);   //!< ???
	UNUSED(bool localize);          //!< Whether or not the task should be localized
	UNUSED(std::string gate_version);  //!< ???
};

class CDMissionTasksTable : public CDTable<CDMissionTasksTable, std::vector<CDMissionTasks>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDMissionTasks> Query(std::function<bool(CDMissionTasks)> predicate);

	std::vector<CDMissionTasks*> GetByMissionID(uint32_t missionID);

	// TODO: Remove this and replace it with a proper lookup function.
	const CDTable::StorageType& GetEntries() const {
		return CDTable::GetEntries();
	}
};

