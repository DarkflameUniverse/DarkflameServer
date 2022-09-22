#pragma once

#ifndef MISSIONTASK_H
#define MISSIONTASK_H

#include "CDMissionTasksTable.h"
#include "MissionTaskType.h"
#include "dCommonVars.h"

class Mission;

/**
 * A task that can be progressed and completed for a mission.
 */
class MissionTask final
{
public:
	MissionTask(Mission* mission, CDMissionTasks* info, uint32_t mask);
	~MissionTask();

	/**
	 * Attempts to progress this task using the provided parameters. Note that the behavior of this method is different
	 * for each mission task type.
	 * @param value the value to progress by
	 * @param associate optional object ID of an entity that was related to the progression
	 * @param targets optional multiple targets that need to be met to progress
	 * @param count a number that indicates the times to progress
	 */
	void Progress(int32_t value, LWOOBJID associate = 0, const std::string& targets = "", int32_t count = 1);

	/**
	 * Returns the current progression of this task
	 * @return the current progression of this task
	 */
	uint32_t GetProgress() const;

	/**
	 * Progresses the progress of this task by the provided value. Does not exceed the target progress.
	 * @param value the value to progress by
	 */
	void AddProgress(int32_t value);

	/**
	 * Sets the progress of the task and optionally notifies the client
	 * @param value the value to set for the progress
	 * @param echo if true, this will notify the client of the change
	 */
	void SetProgress(uint32_t value, bool echo = true);

	/**
	 * Returns the mission this task belongs to
	 * @return the mission this task belongs to
	 */
	Mission* GetMission() const;

	/**
	 * Returns the type of this task
	 * @return the type of this task
	 */
	MissionTaskType GetType() const;

	/**
	 * Returns the value that should be progressed to, to complete the mission (the target value)
	 * @return the target value
	 */
	uint32_t GetTarget() const;

	/**
	 * Returns the database information for this mission
	 * @return the database information for this mission
	 */
	const CDMissionTasks& GetClientInfo() const;

	/**
	 * Returns the mask for this mission, used for communicating updates
	 * @return the mask for this mission, used for communicating updates
	 */
	uint32_t GetMask() const;

	/**
	 * Returns the currently visited list of unique locations (only used for visiting mission types)
	 * @return the currently visited list of unique locations
	 */
	const std::vector<uint32_t>& GetUnique() const;

	/**
	 * Sets the uniquely visited list of locations
	 * @param value the uniquely visited list of locations
	 */
	void SetUnique(const std::vector<uint32_t>& value);

	/**
	 * Returns the possibly target values for this mission task for progression
	 * @return the possibly target values for this mission task for progression
	 */
	const std::vector<uint32_t>& GetTargets() const;

	/**
	 * Returns the parameters for this task: meta information that determines if the task can be progressed. Note:
	 * not used by all task types.
	 * @return the parameters for this task
	 */
	const std::vector<uint32_t>& GetParameters() const;

	/**
	 * Returns all the target values for this mission, including the target value concatenated by the optional list of
	 * targets parsed as ints.
	 * @return all the targets for this task
	 */
	std::vector<uint32_t> GetAllTargets() const;

	/**
	 * Returns whether the value is in the list of target values of this task
	 * @param value the value to check for
	 * @return true if the value is in the target list, false otherwise
	 */
	bool InTargets(uint32_t value) const;

	/**
	 * Returns whether the value is in one of the target values or equals the individual target value of this task
	 * @param value the value to check for
	 * @return true if the value is one of the targets, false otherwise
	 */
	bool InAllTargets(uint32_t value) const;

	/**
	 * Checks if the provided is one of the parameters for this task
	 * @param value the value to check for
	 * @return true if the value is one of the parameters, false otherwise
	 */
	bool InParameters(uint32_t value) const;

	/**
	 * Checks if this task has been completed by comparing its progress against the target value
	 * @return true if the task has been completed, false otherwise
	 */
	bool IsComplete() const;

	/**
	 * Completes the mission by setting the progress to the required value
	 */
	void Complete();

private:

	/**
	 * Datbase information about this task
	 */
	CDMissionTasks* info;

	/**
	 * The mission this task belongs to
	 */
	Mission* mission;

	/**
	 * Mask used for communicating mission updates
	 */
	uint32_t mask;

	/**
	 * The current progression towards the target
	 */
	uint32_t progress;

	/**
	 * The list of target values for progressing this task
	 */
	std::vector<uint32_t> targets;

	/**
	 * The list of parameters for progressing this task (not used by all task types)
	 */
	std::vector<uint32_t> parameters;

	/**
	 * The unique places visited for progression (not used by all task types)
	 */
	std::vector<uint32_t> unique;

	/**
	 * Checks if the task is complete, and if so checks if the parent mission is complete
	 */
	void CheckCompletion() const;
};

#endif
