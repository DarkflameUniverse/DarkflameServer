/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "CDClientManager.h"

#ifndef SCRIPTEDACTIVITYCOMPONENT_H
#define SCRIPTEDACTIVITYCOMPONENT_H

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"

 /**
  * Represents an instance of an activity, having participants and score
  */
class ActivityInstance {
public:
	ActivityInstance(Entity* parent, CDActivities activityInfo) { m_Parent = parent; m_ActivityInfo = activityInfo; };
	//~ActivityInstance();

	/**
	 * Adds an entity to this activity
	 * @param participant the entity to add
	 */
	void AddParticipant(Entity* participant);

	/**
	 * Removes all the participants from this activity
	 */
	void ClearParticipants() { m_Participants.clear(); };

	/**
	 * Starts the instance world for this activity and sends all participants there
	 */
	void StartZone();

	/**
	 * Gives the rewards for completing this activity to some participant
	 * @param participant the participant to give rewards to
	 */
	void RewardParticipant(Entity* participant);

	/**
	 * Removes a participant from this activity
	 * @param participant the participant to remove
	 */
	void RemoveParticipant(const Entity* participant);

	/**
	 * Returns all the participants of this activity
	 * @return all the participants of this activity
	 */
	std::vector<Entity*> GetParticipants() const;

	/**
	 * Currently unused
	 */
	uint32_t GetScore() const;

	/**
	 * Currently unused
	 */
	void SetScore(uint32_t score);
private:

	/**
	 * Currently unused
	 */
	uint32_t score = 0;

	/**
	 * The instance ID of this activity
	 */
	uint32_t m_NextZoneCloneID = 0;

	/**
	 * The database information for this activity
	 */
	CDActivities m_ActivityInfo;

	/**
	 * The entity that owns this activity (the entity that has the ScriptedActivityComponent)
	 */
	Entity* m_Parent;

	/**
	 * All the participants of this activity
	 */
	std::vector<LWOOBJID> m_Participants;
};

/**
 * Represents an entity in a lobby
 */
struct LobbyPlayer {

	/**
	 * The ID of the entity that is in the lobby
	 */
	LWOOBJID entityID;

	/**
	 * Whether or not the entity is ready
	 */
	bool ready = false;

	/**
	 * Returns the entity that is in the lobby
	 * @return the entity that is in the lobby
	 */
	Entity* GetEntity() const;
};

/**
 * Represents a lobby of players with a timer until it should start the activity
 */
struct Lobby {

	/**
	 * The lobby of players
	 */
	std::vector<LobbyPlayer*> players;

	/**
	 * The timer that determines when the activity should start
	 */
	float timer;
};

/**
 * Represents the score for the player in an activity, one index might represent score, another one time, etc.
 */
struct ActivityPlayer {

	/**
	 * The entity that the score is tracked for
	 */
	LWOOBJID playerID;

	/**
	 * The list of score for this entity
	 */
	float values[10];
};

/**
 * Welcome to the absolute behemoth that is the scripted activity component. I have now clue how this was managed in
 * live but I figure somewhat similarly and it's terrible. In a nutshell, this components handles any activity that
 * can be done in the game from quick builds to boss fights to races. On top of that, this component handles instancing
 * and lobbying.
 */
class ScriptedActivityComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_SCRIPTED_ACTIVITY;

	ScriptedActivityComponent(Entity* parent, int activityID);
	~ScriptedActivityComponent() override;

	void Update(float deltaTime) override;
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) const;

	/**
	 * Makes some entity join the minigame, if it's a lobbied one, the entity will be placed in the lobby
	 * @param player the entity to join the game
	 */
	void PlayerJoin(Entity* player);

	/**
	 * Makes an entity join the lobby for this minigame, if it exists
	 * @param player the entity to join
	 */
	void PlayerJoinLobby(Entity* player);

	/**
	 * Makes the player leave the lobby
	 * @param playerID the entity to leave the lobby
	 */
	void PlayerLeave(LWOOBJID playerID);

	/**
	 * Removes the entity from the minigame (and its score)
	 * @param playerID the entity to remove from the minigame
	 */
	void PlayerRemove(LWOOBJID playerID);

	/**
	 * Adds all the players to an instance of some activity
	 * @param instance the instance to load the players into
	 * @param lobby the players to load into the instance
	 */
	void LoadPlayersIntoInstance(ActivityInstance* instance, const std::vector<LobbyPlayer*>& lobby) const;

	/**
	 * Removes a lobby from the activity manager
	 * @param lobby the lobby to remove
	 */
	void RemoveLobby(Lobby* lobby);

	/**
	 * Marks a player as (un)ready in a lobby
	 * @param player the entity to mark
	 * @param bReady true if the entity is ready, false otherwise
	 */
	void PlayerReady(Entity* player, bool bReady);

	/**
	 * Returns the ID of this activity
	 * @return the ID of this activity
	 */
	int GetActivityID() { return m_ActivityInfo.ActivityID; }

	/**
	 * Returns if this activity has a lobby, e.g. if it needs to instance players to some other map
	 * @return true if this activity has a lobby, false otherwise
	 */
	bool HasLobby() const;

	/**
	 * Checks if a player is currently waiting in a lobby
	 * @param player the entity to check for
	 * @return true if the entity is waiting in a lobby, false otherwise
	 */
	bool PlayerIsInQueue(Entity* player);

	/**
	 * Checks if an entity is currently playing this activity
	 * @param player the entity to check
	 * @return true if the entity is playing this lobby, false otherwise
	 */
	bool IsPlayedBy(Entity* player) const;

	/**
	 * Checks if an entity is currently playing this activity
	 * @param playerID the entity to check
	 * @return true if the entity is playing this lobby, false otherwise
	 */
	bool IsPlayedBy(LWOOBJID playerID) const;

	/**
	 * Legacy: used to check for unimplemented maps, gladly, this now just returns true :)
	 */
	bool IsValidActivity(Entity* player);

	/**
	 * Removes the cost of the activity (e.g. green imaginate) for the entity that plays this activity
	 * @param player the entity to take cost for
	 * @return true if the cost was successfully deducted, false otherwise
	 */
	bool TakeCost(Entity* player) const;

	/**
	 * Handles any response from a player clicking on a lobby / instance menu
	 * @param player the entity that clicked
	 * @param id the message that was passed
	 */
	void HandleMessageBoxResponse(Entity* player, const std::string& id);

	/**
	 * Creates a new instance for this activity
	 * @return a new instance for this activity
	 */
	ActivityInstance* NewInstance();

	/**
	 * Returns all the currently active instances of this activity
	 * @return all the currently active instances of this activity
	 */
	const std::vector<ActivityInstance*>& GetInstances() const;

	/**
	 * Returns the instance that some entity is currently playing in
	 * @param playerID the entity to check for
	 * @return if any, the instance that the entity is currently in
	 */
	ActivityInstance* GetInstance(const LWOOBJID playerID);

	/**
	 * Removes all the instances
	 */
	void ClearInstances();

	/**
	 * Returns all the score for the players that are currently playing this activity
	 * @return
	 */
	std::vector<ActivityPlayer*> GetActivityPlayers() { return m_ActivityPlayers; };

	/**
	 * Returns activity data for a specific entity (e.g. score and such).
	 * @param playerID the entity to get data for
	 * @return the activity data (score) for the passed player in this activity, if it exists
	 */
	ActivityPlayer* GetActivityPlayerData(LWOOBJID playerID);

	/**
	 * Sets some score value for an entity
	 * @param playerID the entity to set score for
	 * @param index the score index to set
	 * @param value the value to set in for that index
	 */
	void SetActivityValue(LWOOBJID playerID, uint32_t index, float_t value);

	/**
	 * Returns activity score for the passed parameters
	 * @param playerID the entity to get score for
	 * @param index the index to get score for
	 * @return activity score for the passed parameters
	 */
	float_t GetActivityValue(LWOOBJID playerID, uint32_t index);

	/**
	 * Removes activity score tracking for some entity
	 * @param playerID the entity to remove score for
	 */
	void RemoveActivityPlayerData(LWOOBJID playerID);

	/**
	 * Adds activity score tracking for some entity
	 * @param playerID the entity to add the activity score for
	 * @return the created entry
	 */
	ActivityPlayer* AddActivityPlayerData(LWOOBJID playerID);

	/**
	 * Sets the mapID that this activity points to
	 * @param mapID the map ID to set
	 */
	void SetInstanceMapID(uint32_t mapID) { m_ActivityInfo.instanceMapID = mapID; };

	/**
	 * Returns the LMI that this activity points to for a team size
	 * @param teamSize the team size to get the LMI for
	 * @return the LMI that this activity points to for a team size
	 */
	uint32_t GetLootMatrixForTeamSize(uint32_t teamSize) { return m_ActivityLootMatrices[teamSize]; }
private:

	/**
	 * The database information for this activity
	 */
	CDActivities m_ActivityInfo;

	/**
	 * All the active instances of this activity
	 */
	std::vector<ActivityInstance*> m_Instances;

	/**
	 * The current lobbies for this activity
	 */
	std::vector<Lobby*> m_Queue;

	/**
	 * All the activity score for the players in this activity
	 */
	std::vector<ActivityPlayer*> m_ActivityPlayers;

	/**
	 * LMIs for team sizes
	 */
	std::unordered_map<uint32_t, uint32_t> m_ActivityLootMatrices;
};

#endif // SCRIPTEDACTIVITYCOMPONENT_H
