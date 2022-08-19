#pragma once

#ifndef MISSION_H
#define MISSION_H

#include <vector>
#include <string>

#include "CDMissionsTable.h"
#include "MissionTask.h"
#include "dCommonVars.h"
#include "Entity.h"
#include "MissionState.h"
#include "MissionLockState.h"

class MissionComponent;

/**
 * A mission (or achievement) that a player may unlock, progress and complete.
 */
class Mission final
{
public:
	Mission(MissionComponent* missionComponent, uint32_t missionId);
	~Mission();

	void LoadFromXml(tinyxml2::XMLElement* element);
	void UpdateXml(tinyxml2::XMLElement* element);

	/**
	 * Returns the ID of this mission
	 * @return the ID of this mission
	 */
	uint32_t GetMissionId() const;

	/**
	 * Returns the entity that is currently progressing this mission
	 * @return the entity that is currently progressing this mission
	 */
	Entity* GetAssociate() const;

	/**
	 * Returns the account owns the entity that is currently progressing this mission
	 * @return the account owns the entity that is currently progressing this mission
	 */
	User* GetUser() const;

	/**
	 * Returns the current state of this mission
	 * @return the current state of this mission
	 */
	MissionState GetMissionState() const;

	/**
	 * Returns the database information that represents to this mission.
	 * @return the database information that represents to this mission.
	 */
	const CDMissions& GetClientInfo() const;

	/**
	 * Returns the number of times the entity has completed this mission, can only be > 0 for dailies.
	 * @return the number of thimes the entity has completed this mission
	 */
	uint32_t GetCompletions() const;

	/**
	 * Sets the number of times this mission has been completed
	 * @param value the number of times this mission should be completed
	 */
	void SetCompletions(uint32_t value);

	/**
	 * Returns the last timestamp at which the entity completed this mission
	 * @return the last timestamp at which the entity completed this mission
	 */
	uint32_t GetTimestamp() const;

	/**
	 * Returns some specific reward that should be returned from the possible rewards indicated by the client
	 * @return some specific reward that should be returned from the possible rewards indicated by the client
	 */
	LOT GetReward() const;

	/**
	 * Sets an some specific reward that should be returned from the possible rewards indicated by the client
	 * @param lot the reward to set
	 */
	void SetReward(LOT lot);

	/**
	 * Returns all the tasks that must be completed to mark this mission as complete
	 * @return all the tasks that must be completed to mark this mission as complete
	 */
	std::vector<MissionTask*> GetTasks() const;

	/**
	 * Updates the mission state to the one provided
	 * @param state the mission state to set
	 * @param sendingRewards a flag indicating to the client that rewards wil lfollow
	 */
	void SetMissionState(MissionState state, bool sendingRewards = false);

	/**
	 * Currently unimplemented
	 */
	void SetMissionTypeState(MissionLockState state, const std::string& type, const std::string& subType);

	/**
	 * Returns whether this mission is an achievement
	 * @return true if this mission is an achievement, false otherwise
	 */
	bool IsAchievement() const;

	/**
	 * Returns whether this mission is a mission (e.g.: not an achievement)
	 * @return true if this mission is not an achievement, false otherwise
	 */
	bool IsMission() const;

	/**
	 * Returns whether this mission can be repeated (mostly used for dailies)
	 * @return true if this mission can be repeated, false otherwise
	 */
	bool IsRepeatable() const;

	/**
	 * Returns whether the entity has completed this mission before
	 * @return true if the mission has been completed before, false otherwise
	 */
	bool IsComplete() const;

	/**
	 * Returns whether the mission is currently active
	 * @return true if the mission is currently active, false otherwise
	 */
	bool IsActive() const;

	/**
	 * Sets the mission state to active, takes into account if this is a repeatable mission.
	 */
	void MakeActive();

	/**
	 * Returns whether the entity has completed all tasks and can hand the mission in for rewards.
	 * @return true if the entity can hand the mission in, false otherwise
	 */
	bool IsReadyToComplete() const;

	/**
	 * Sets the mission state to ready to complete, takes into account if this is a repeatable mission
	 */
	void MakeReadyToComplete();

	/**
	 * Returns whether this mission can be accepted by the entity
	 * @return true if the mission can be accepted by the entity, false otherwise
	 */
	bool IsAvalible() const;

	/**
	 * Sets the mission state to available, takes into account if this mission is repeatable
	 */
	void MakeAvalible();

	/**
	 * Returns whether this mission is one where an entity simply has to go somewhere, but doesn't have to turn in the
	 * mission tasks at the original mission giver (called a fetch mission).
	 * @return true if this is a fetch mission, false otherwise
	 */
	bool IsFetchMission() const;

	/**
	 * Accepts this mission, setting it to available. Also progresses any of the tasks if the entity has already
	 * progressed for them (for example "collect X bricks", will fast track for the amount of bricks the entity
	 * already has).
	 */
	void Accept();

	/**
	 * Completes the mission and handles all logistics regarding that: checking all tasks, handing out rewards,
	 * emailing them if the inventory is full, etc. If the mission tasks have not all been completed this is a no-op.
	 * @param yieldRewards if true, rewards will be given to the entity
	 */
	void Complete(bool yieldRewards = true);

	/**
	 * Checks if this mission is ready to be completed and updates the state if so. If this is an achievement, the
	 * state will automatically be updated to completed as there's nobody to hand achievements in to.
	 */
	void CheckCompletion();

	/**
	 * Gives all the rewards (items, score, stats, etc.) to the entity. Takes into account if the entity has completed
	 * the mission before.
	 */
	void YieldRewards();

	/**
	 * Attempts to progress tasks of a certain type for this mission. Note that the interpretation of any of these
	 * arguments is up to the mission task at hand.
	 * @param type the mission task type to progress
	 * @param value the value to progress the mission task with
	 * @param associate optional object ID that was related to the progression
	 * @param targets optional multiple targets that need to be met for progression
	 * @param count optional count to progress with
	 */
	void Progress(MissionTaskType type, int32_t value, LWOOBJID associate = 0, const std::string& targets = "", int32_t count = 1);

	/**
	 * Returns if the mission ID that's given belongs to an existing mission
	 * @param missionId the mission ID to check for
	 * @return true if the mission exists, false otherwise
	 */
	static bool IsValidMission(uint32_t missionId);

	/**
	 * Returns if the mission ID that's given belongs to an existing mission
	 * @param missionId the mission ID to check for
	 * @param info variable to store the queried mission information in
	 * @return true if the mission exists, false otherwise
	 */
	static bool IsValidMission(uint32_t missionId, CDMissions& info);

	/**
	 * @brief Returns the unique mission order ID
	 * 
	 * @return The unique order ID
	 */
	uint32_t GetUniqueMissionOrderID() { return m_UniqueMissionID; };

	/**
	 * Sets the unique mission order ID of this mission
	 */
	void SetUniqueMissionOrderID(uint32_t value) { m_UniqueMissionID = value; };
private:
	/**
	 * Progresses all the newly accepted tasks for this mission after it has been accepted to reflect the state of the
	 * inventory of the entity.
	 */
	void Catchup();

	/**
	 * The database information that corresponds to this mission
	 */
	const CDMissions* info;

	/**
	 * The current state this mission is in
	 */
	MissionState m_State;

	/**
	 * The number of times the entity has completed this mission
	 */
	uint32_t m_Completions;

	/**
	 * The last time the entity completed this mission
	 */
	uint32_t m_Timestamp;

	/**
	 * The mission component of the entity that owns this mission
	 */
	MissionComponent* m_MissionComponent;

	/**
	 * Optionally specific reward that should be returned from the possible rewards indicated by the client
	 */
	LOT m_Reward;

	/**
	 * All the tasks that can be progressed for this mission
	 */
	std::vector<MissionTask*> m_Tasks;

	/**
	 * The unique ID what order this mission was accepted in.
	 */
	uint32_t m_UniqueMissionID;
};

#endif
