#pragma once

// Custom Classes
#include "CDTable.h"

struct CDMissionTasks {
	unsigned int id;                //!< The Mission ID that the task belongs to
	UNUSED(unsigned int locStatus);         //!< ???
	unsigned int taskType;          //!< The task type
	unsigned int target;            //!< The mission target
	std::string targetGroup;        //!< The mission target group
	int targetValue;       //!< The target value
	std::string taskParam1;    //!< The task param 1
	UNUSED(std::string largeTaskIcon); //!< ???
	UNUSED(unsigned int IconID);            //!< ???
	unsigned int uid;               //!< ???
	UNUSED(unsigned int largeTaskIconID);   //!< ???
	UNUSED(bool localize);          //!< Whether or not the task should be localized
	UNUSED(std::string gate_version);  //!< ???
};

namespace CDMissionTasksTable {
private:
	std::vector<CDMissionTasks> entries;

public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDMissionTasks> Query(std::function<bool(CDMissionTasks)> predicate);

	std::vector<CDMissionTasks*> GetByMissionID(uint32_t missionID);

	const std::vector<CDMissionTasks>& GetEntries(void) const;
};

