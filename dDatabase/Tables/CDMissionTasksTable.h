#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDMissionTasksTable.hpp
 \brief Contains data for the MissionTasks table
 */

 //! ObjectSkills Struct
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

//! ObjectSkills table
class CDMissionTasksTable : public CDTable {
private:
	std::vector<CDMissionTasks> entries;

public:

	//! Constructor
	CDMissionTasksTable(void);

	//! Destructor
	~CDMissionTasksTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	 \param predicate The predicate
	 */
	std::vector<CDMissionTasks> Query(std::function<bool(CDMissionTasks)> predicate);

	std::vector<CDMissionTasks*> GetByMissionID(uint32_t missionID);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	const std::vector<CDMissionTasks>& GetEntries(void) const;
};

