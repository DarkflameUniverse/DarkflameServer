#ifndef ACTIVITYCOMPONENT_H
#define ACTIVITYCOMPONENT_H

#include "CDClientManager.h"
#include "BitStream.h"
#include "Entity.h"
#include "Component.h"
#include "eReplicaComponentType.h"

#include "CDActivitiesTable.h"
#include <array>

namespace GameMessages {
	class GameMsg;
};

namespace ChatPackets {
	struct MatchTransfer;
};

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
	 * Sends the participants to the activity's instance that master started (the chat server's matchmaking found it)
	 * @param transfer the instance's zone, address and the players chat sent here
	 */
	void TransferParticipants(const ChatPackets::MatchTransfer& transfer);

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

	[[nodiscard]] uint32_t GetNextZoneCloneID() const noexcept { return m_NextZoneCloneID; }

	const CDActivities& GetActivityInfo() const noexcept { return m_ActivityInfo; }
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
	CDActivities m_ActivityInfo{};

	/**
	 * The entity that owns this activity (the entity that has the ScriptedActivityComponent)
	 */
	Entity* m_Parent{};

	/**
	 * All the participants of this activity
	 */
	std::vector<LWOOBJID> m_Participants;
};

/**
 * Represents the score for the player in an activity, one index might represent score, another one time, etc.
 */
struct ActivityPlayer {

	/**
	 * The entity that the score is tracked for
	 */
	LWOOBJID playerID{};

	/**
	 * The list of score for this entity
	 */
	float values[10]{};
};

/**
 * Welcome to the absolute behemoth that is the scripted activity component. I have now clue how this was managed in
 * live but I figure somewhat similarly and it's terrible. In a nutshell, this components handles any activity that
 * can be done in the game from quick builds to boss fights to races. On top of that, this component handles instancing
 * and lobbying.
 */
class ActivityComponent : public Component {
public:
	ActivityComponent(Entity* parent, int32_t activityID);

	void LoadActivityData(const int32_t activityId);

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	/**
	 * Makes some entity join the minigame, if it's a lobbied one, the entity will be placed in the lobby
	 * @param player the entity to join the game
	 * @param playerChoices the name-value text of the client's MatchRequest (a racing car), shown to the lobby
	 */
	void PlayerJoin(Entity* player, const std::string& playerChoices = "");

	/**
	 * Makes an entity join the lobby for this minigame. The lobbies are the chat server's, across every world
	 * (docs/Matchmaking.md).
	 * @param player the entity to join
	 * @param playerChoices the name-value text of the client's MatchRequest
	 */
	void PlayerJoinLobby(Entity* player, const std::string& playerChoices = "");

	/**
	 * Makes the player leave the lobby (the chat server's)
	 * @param playerID the entity to leave the lobby
	 */
	void PlayerLeave(LWOOBJID playerID);

	/**
	 * Removes the entity from the minigame (and its score)
	 * @param playerID the entity to remove from the minigame
	 */
	void PlayerRemove(LWOOBJID playerID);

	/**
	 * A lobby's match for this activity has its instance: the players of it in this world who can pay go there
	 * @param transfer the instance and the players, from the chat server
	 */
	void StartMatch(const ChatPackets::MatchTransfer& transfer);

	/**
	 * Marks a player as (un)ready in the lobby they wait in (the chat server knows which)
	 * @param player the entity to mark
	 * @param bReady true if the entity is ready, false otherwise
	 */
	static void PlayerReady(Entity* player, bool bReady);

	/**
	 * Returns the ID of this activity
	 * @return the ID of this activity
	 */
	int GetActivityID() { return m_ActivityInfo.ActivityID; }

	// Whether or not team loot should be dropped on death for this activity
	// if true, and a player is supposed to get loot, they are skipped
	bool GetNoTeamLootOnDeath() const { return m_ActivityInfo.noTeamLootOnDeath; }

	/**
	 * Returns if this activity has a lobby, e.g. if it needs to instance players to some other map
	 * @return true if this activity has a lobby, false otherwise
	 */
	bool HasLobby() const;

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
	 * Checks if the entity has enough cost to play this activity
	 * @param player the entity to check
	 * @return true if the entity has enough cost to play this activity, false otherwise
	*/
	bool CheckCost(Entity* player) const;

	/**
	 * Removes the cost of the activity (e.g. green imaginate) for the entity that plays this activity
	 * @param player the entity to take cost for
	 * @return true if the cost was taken, false otherwise
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
	ActivityInstance& NewInstance();

	/**
	 * Returns the instance that some entity is currently playing in
	 * @param playerID the entity to check for
	 * @return if any, the instance that the entity is currently in
	 */
	const ActivityInstance& GetInstance(const LWOOBJID playerID) const;

	/**
	 * @brief Reloads the config settings for this component
	 *
	 */
	void ReloadConfig();

	/**
	 * Returns activity data for a specific entity (e.g. score and such).
	 * @param playerID the entity to get data for
	 * @return the activity data (score) for the passed player in this activity, if it exists
	 */
	bool PlayerHasActivityData(LWOOBJID playerID) const;

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
	float_t GetActivityValue(LWOOBJID playerID, uint32_t index) const;

	/**
	 * Removes activity score tracking for some entity
	 * @param playerID the entity to remove score for
	 */
	void RemoveActivityPlayerData(LWOOBJID playerID);

	/**
	 * Sets the mapID that this activity points to
	 * @param mapID the map ID to set
	 */
	void SetInstanceMapID(uint32_t mapID) { m_ActivityInfo.instanceMapID = mapID; };

private:
	bool OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& msg);
	/**
	 * The database information for this activity
	 */
	CDActivities m_ActivityInfo{};

	/**
	 * All the active instances of this activity
	 */
	std::vector<ActivityInstance> m_Instances;

	/**
	 * All the activity score for the players in this activity
	 */
	std::map<LWOOBJID, std::array<float, 10>> m_ActivityPlayers;

	/**
	 * The activity id
	 */
	int32_t m_ActivityID;

	/**
	 * If the Activity info is dirty
	*/
	bool m_DirtyActivityInfo = true;
};

#endif // ACTIVITYCOMPONENT_H
