/*
 * Darkflame Universe
 * Copyright 2019
 */

#ifndef MISSIONCOMPONENT_H
#define MISSIONCOMPONENT_H

#include <unordered_map>
#include <vector>
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "Entity.h"
#include "Mission.h"
#include "tinyxml2.h"
#include "CDClientManager.h"
#include "CDMissionsTable.h"
#include "Component.h"

 /**
  * The mission inventory of an entity. Tracks mission state for each mission that can be accepted and allows for
  * progression of each of the mission task types (see MissionTaskType).
  */
class MissionComponent : public Component
{
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_MISSION;

	explicit MissionComponent(Entity* parent);
	~MissionComponent() override;
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	void LoadFromXml(tinyxml2::XMLDocument* doc) override;
	void UpdateXml(tinyxml2::XMLDocument* doc) override;

	/**
	 * Returns all the missions for this entity, mapped by mission ID
	 * @return the missions for this entity, mapped by mission ID
	 */
	const std::unordered_map<uint32_t, Mission*>& GetMissions() const;

	/**
	 * Returns the mission for the given mission ID, if it exists
	 * @param missionId the id of the mission to get
	 * @return the mission for the given mission ID
	 */
	Mission* GetMission(uint32_t missionId) const;

	/**
	 * Returns the current state of the entities progression for the mission of the specified ID
	 * @param missionId the ID of the mission to get the mission state for
	 * @return the mission state of the mission specified by the ID
	 */
	MissionState GetMissionState(uint32_t missionId) const;

	/**
	 * Checks if the entity has all the requirements for accepting the mission specified by the ID.
	 * @param missionId the mission ID to check for if the character may accept it
	 * @return whether this entity can accept the mission represented by the given mission ID
	 */
	bool CanAccept(uint32_t missionId) const;

	/**
	 * Accepts the mission specified by the ID, if the entity may accept it. Also stores it in the mission inventory.
	 * @param missionId the ID of the mission to accept
	 * @param skipChecks skips the checks for the mission prerequisites
	 */
	void AcceptMission(uint32_t missionId, bool skipChecks = false);

	/**
	 * Completes the mission specified by the given ID, if the entity has fulfilled all progress requirements.
	 * @param missionId the ID of the mission to complete
	 * @param skipChecks skips the checks for having completed all of the mission tasks
	 * @param yieldRewards whether to yield mission rewards, currently unused
	 */
	void CompleteMission(uint32_t missionId, bool skipChecks = false, bool yieldRewards = true);

	/**
	 * Removes the mission from the entities' mission chain. Not used for normal gameplay but useful for debugging.
	 * @param missionId the ID of the mission to remove
	 */
	void RemoveMission(uint32_t missionId);

	/**
	 * Attempts to progress mission tasks for a given type using parameters to progress. Note that this function is
	 * very abstract and different mission tasks require different combinations of parameters. This also progresses
	 * achievements, which are basically just missions with the isMission flag set to false.
	 * @param type the type of the mission task to try and progress with
	 * @param value the value to progress with, could be a LOT for item collection for example
	 * @param associate an associated entity for the progression, might be an activity host for example
	 * @param targets optionally multiple target values that could exist for the mission that we wish to progress
	 * @param count the number to progress by, for example the number of items
	 * @param ignoreAchievements do not progress achievements
	 */
	void Progress(MissionTaskType type, int32_t value, LWOOBJID associate = 0, const std::string& targets = "", int32_t count = 1, bool ignoreAchievements = false);

	/**
	 * Forces progression for a mission and task, ignoring checks
	 * @param missionId the mission ID to try and progress
	 * @param taskId the task ID of the task belonging to the mission trying to progress
	 * @param value the value to progress with
	 * @param acceptMission accept the mission if it was not already accepted
	 */
	void ForceProgress(uint32_t missionId, uint32_t taskId, int32_t value, bool acceptMission = true);

	/**
	 * Forces progress for all tasks of a certain type that belong to the same mission
	 * @param missionId the mission to progress
	 * @param taskType the task tyoe to progress
	 * @param value the value to progress with
	 * @param acceptMission accept the mission if it wasn't already
	 */
	void ForceProgressTaskType(uint32_t missionId, uint32_t taskType, int32_t value, bool acceptMission = true);

	/**
	 * Force progresses by checking the value and progressing by 1
	 * @param missionId the mission to progress
	 * @param taskType the task to progress for
	 * @param value the value to check the mission values before progressing
	 * @param acceptMission accept the mission if it wasn't already
	 */
	void ForceProgressValue(uint32_t missionId, uint32_t taskType, int32_t value, bool acceptMission = true);

	/**
	 * Returns client database mission information for a mission
	 * @param missionId the ID of the mission to get the info for
	 * @param result the result to store the information in
	 * @return true if the information was succesfully retrieved, false otherwise
	 */
	bool GetMissionInfo(uint32_t missionId, CDMissions& result);

	/**
	 * Checks if there's any achievements we might be able to accept for the given parameters
	 * @param type the task type for tasks in the achievement that we wish to progress
	 * @param value the value to progress by
	 * @param progress if we can accept the mission, this will apply the progression
	 * @param associate optional associate related to mission progression
	 * @param targets optional multiple targets related to mission progression
	 * @param count the number of values to progress by (differs by task type)
	 * @return true if a achievement was accepted, false otherwise
	 */
	bool LookForAchievements(MissionTaskType type, int32_t value, bool progress = true, LWOOBJID associate = LWOOBJID_EMPTY, const std::string& targets = "", int32_t count = 1);

	/**
	 * Checks if there's a mission active that requires the collection of the specified LOT
	 * @param lot the LOT to check for
	 * @return if there's a mission active that requires the collection of the specified LOT
	 */
	bool RequiresItem(LOT lot);

	/**
	 * Collects a collectable for the entity, unrendering it for the entity
	 * @param collectibleID the ID of the collectable to add
	 */
	void AddCollectible(int32_t collectibleID);

	/**
	 * Checks if the entity already has a collectible of the specified ID
	 * @param collectibleID the ID of the collectible to check
	 * @return if the entity already has a collectible of the specified ID
	 */
	bool HasCollectible(int32_t collectibleID);

	/**
	 * Checks if the entity has a certain mission in its inventory
	 * @param missionId the ID of the mission to check
	 * @return if the entity has a certain mission in its inventory
	 */
	bool HasMission(uint32_t missionId);

private:
	/**
	 * All the missions owned by this entity, mapped by mission ID
	 */
	std::unordered_map<uint32_t, Mission*> m_Missions;

	/**
	 * All the collectibles currently collected by the entity
	 */
	std::vector<int32_t> m_Collectibles;


	/**
	 * For the given parameters, finds the mission IDs of the achievements that may be unlcoked
	 * @param type the mission task type to try and progress
	 * @param value the value to try and progress with
	 * @param targets optional targets to progress with
	 * @return list of mission IDs (achievements) that can be progressed for the given parameters
	 */
	static const std::vector<uint32_t>& QueryAchievements(MissionTaskType type, int32_t value, const std::string targets);

	/**
	 * As achievements can be hard to query, we here store a list of all the mission IDs that can be unlocked for a
	 * combination of tasks and values, so that they can be easily re-queried later
	 */
	static std::unordered_map<size_t, std::vector<uint32_t>> m_AchievementCache;

	/**
	 * Order of missions in the UI.  This value is incremented by 1
	 * for each mission the Entity that owns this component accepts.
	 * In live this value started at 745.
	 */
	uint32_t m_LastUsedMissionOrderUID = 746U;
};

#endif // MISSIONCOMPONENT_H
