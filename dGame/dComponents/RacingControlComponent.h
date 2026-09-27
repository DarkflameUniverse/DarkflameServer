/**
 * Thanks to Simon for his early research on the racing system.
 */

#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "ActivityComponent.h"
#include "eReplicaComponentType.h"
#include <chrono>

 /**
  * Information for each player in the race
  */
struct RacingPlayerInfo {

	/**
	 * The ID of the player
	 */
	LWOOBJID playerID;

	/**
	 * The ID of the car the player is driving
	 */
	LWOOBJID vehicleID;

	/**
	 * The index of this player in the list of players
	 */
	uint32_t playerIndex;

	/**
	 * Whether the player has finished loading or not
	 */
	bool playerLoaded;

	/**
	 * Scripted activity component score
	 */
	float data[10]{};

	/**
	 * Point that the player will respawn at if they smash their car
	 */
	NiPoint3 respawnPosition;

	/**
	 * Rotation that the player will respawn at if they smash their car
	 */
	NiQuaternion respawnRotation = QuatUtils::IDENTITY;

	/**
	 * The index in the respawn point the player is now at
	 */
	uint32_t respawnIndex;

	/**
	 * The number of laps the player has completed
	 */
	uint32_t lap;

	/**
	 * Whether or not the player has finished the race
	 */
	uint32_t finished;

	/**
	 * Unused
	 */
	uint16_t reachedPoints;

	/**
	 * The fastest lap time of the player
	 */
	std::chrono::milliseconds bestLapTime;

	/**
	 * The current lap time of the player
	 */
	std::chrono::high_resolution_clock::time_point lapTime;

	/**
	 * The number of times this player smashed their car
	 */
	uint32_t smashedTimes = 0;

	/**
	 * Whether or not the player should be smashed if the game is reloaded
	 */
	bool noSmashOnReload = false;

	/**
	 * Whether or not this player has collected their rewards from completing the race
	 */
	bool collectedRewards = false;

	/**
	 * Unused
	 */
	std::chrono::milliseconds raceTime;

	/**
	 * The reset planes (path waypoints) the player is between, and how many planes they have driven back through
	 * since they last went forward through one, tracked the way the client tracks them
	 * (LWORacingControlComponent::HandlePlayerAddedToRace @ 00cba0c0 starts them at 0 and 1).
	 */
	uint32_t lastPlane = 0;
	uint32_t upcomingPlane = 1;
	uint32_t wrongWayCount = 0;

	/**
	 * Seconds left until a player going the wrong way is put back on the track, negative when not counting down
	 */
	float wrongWayTime = -1.0f;
};

/**
 * Component that's attached to a manager entity in each race zone that loads player vehicles, keep scores, etc.
 */
struct Path;
struct PathWaypoint;

class RacingControlComponent final : public ActivityComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::RACING_CONTROL;

	RacingControlComponent(Entity* parentEntity, const int32_t componentID);
	~RacingControlComponent();

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;
	void Update(float deltaTime) override;

	/**
	 * Invoked when a player loads into the zone.
	 */
	void OnPlayerLoaded(Entity* player);

	/**
	 * Moves the player's reset planes as they drive through them and puts them back on the track when they have been
	 * going the wrong way for as long as the client counts down.
	 */
	void UpdateWrongWay(RacingPlayerInfo& player, const Entity& vehicle, const Path& path, float deltaTime);

	/**
	 * Moves the player's reset planes for the car being at position, like the client does each frame, and starts the
	 * wrong way countdown when they have driven back through a second plane.
	 */
	static void StepResetPlanes(RacingPlayerInfo& player, const std::vector<PathWaypoint>& waypoints, const NiPoint3& position);

	/**
	 * Initalize the player's vehicle.
	 *
	 * @param player The player who's vehicle to initialize.
	 * @param initialLoad Is this the first time the player is loading in this race?
	 */
	void LoadPlayerVehicle(Entity* player, uint32_t positionNumber, bool initialLoad = false);

	/**
	 * Invoked when the client says it has loaded in.
	 */
	void OnRacingClientReady(Entity* player);

	/**
	 * Invoked when the client says it should be smashed.
	 */
	void OnRequestDie(Entity* player, const std::u16string& deathType = u"");

	/**
	 * Invoked when the player has finished respawning.
	 */
	void OnRacingPlayerInfoResetFinished(Entity* player);

	/**
	 * Invoked when the player responds to the GUI.
	 */
	void HandleMessageBoxResponse(Entity* player, int32_t button, const std::string& id);

	/**
	 * Get the racing data from a player's LWOOBJID.
	 */
	RacingPlayerInfo* GetPlayerData(LWOOBJID playerID);

	bool MsgConfigureRacingControl(const GameMessages::ConfigureRacingControl& msg);

private:

	/**
	 * The players that are currently racing
	 */
	std::vector<RacingPlayerInfo> m_RacingPlayers;

	/**
	 * The paths that are followed for the camera scenes
	 * Configurable in the ConfigureRacingControl msg with the key `Race_PathName`.
	 */
	std::u16string m_PathName;

	/**
	 * The ID of the activity for participating in this race
	 * Configurable in the ConfigureRacingControl msg with the key `activityID`.
	 */
	uint32_t m_ActivityID;

	/**
	 * The world the players return to when they finish the race
	 */
	uint32_t m_MainWorld;

	/**
	 * The number of laps that are remaining for the winning player
	 */
	uint16_t m_RemainingLaps;

	/**
	 * The ID of the player that's currently winning the race
	 */
	LWOOBJID m_LeadingPlayer;

	/**
	 * The overall best lap from all the players
	 */
	float m_RaceBestLap;

	/**
	 * The overall best time from all the players
	 */
	float m_RaceBestTime;

	/**
	 * Whether or not the race has started
	 */
	bool m_Started;

	/**
	 * The time left until the race will start
	 */
	float m_StartTimer;

	/**
	 * The time left for loading the players
	 */
	float m_LoadTimer;

	/**
	 * Whether or not all players have loaded
	 */
	bool m_Loaded;

	/**
	 * The number of loaded players
	 */
	uint32_t m_LoadedPlayers;

	/**
	 * All the players that are in the lobby, loaded or not
	 */
	std::vector<LWOOBJID> m_LobbyPlayers;

	/**
	 * The number of players that have finished the race
	 */
	uint32_t m_Finished;

	/**
	 * The time the race was started
	 */
	std::chrono::high_resolution_clock::time_point m_StartTime;

	/**
	 * Timer for tracking how long a player was alone in this race
	 */
	float m_EmptyTimer;

	bool m_SoloRacing;

	/**
	 * Value for message box response to know if we are exiting the race via the activity dialogue
	 */
	const int32_t m_ActivityExitConfirm = 1;

	bool m_AllPlayersReady = false;

	/**
	 * @brief The number of laps in this race. Configurable in the ConfigureRacingControl msg
	 * with the key `Number_of_Laps`.
	 * 
	 */
	int32_t m_NumberOfLaps{ 3 };

	/**
	 * @brief The minimum number of players required to progress group achievements.
	 * Configurable with the ConfigureRacingControl msg with the key `Minimum_Players_for_Group_Achievements`.
	 * 
	 */
	int32_t m_MinimumPlayersForGroupAchievements{ 2 };
};
