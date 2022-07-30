/**
 * Thanks to Simon for his early research on the racing system.
 */

#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"

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
	NiQuaternion respawnRotation;

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
	time_t bestLapTime = 0;

	/**
	 * The current lap time of the player
	 */
	time_t lapTime = 0;

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
	time_t raceTime = 0;
};

/**
 * Component that's attached to a manager entity in each race zone that loads player vehicles, keep scores, etc.
 */
class RacingControlComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_RACING_CONTROL;

	RacingControlComponent(Entity* parentEntity);
	~RacingControlComponent();

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	void Update(float deltaTime);

	/**
	 * Invoked when a player loads into the zone.
	 */
	void OnPlayerLoaded(Entity* player);

	/**
	 * Initalize the player's vehicle.
	 *
	 * @param player The player who's vehicle to initialize.
	 * @param initialLoad Is this the first time the player is loading in this race?
	 */
	void LoadPlayerVehicle(Entity* player, bool initialLoad = false);

	/**
	 * Invoked when the client says it has loaded in.
	 */
	void OnRacingClientReady(Entity* player);

	/**
	 * Invoked when the client says it should be smashed.
	 */
	void OnRequestDie(Entity* player);

	/**
	 * Invoked when the player has finished respawning.
	 */
	void OnRacingPlayerInfoResetFinished(Entity* player);

	/**
	 * Invoked when the player responds to the GUI.
	 */
	void HandleMessageBoxResponse(Entity* player, const std::string& id);

	/**
	 * Get the racing data from a player's LWOOBJID.
	 */
	RacingPlayerInfo* GetPlayerData(LWOOBJID playerID);

	/**
	 * Formats a time to a string, currently unused
	 * @param time the time to format
	 * @return the time formatted as string
	 */
	static std::string FormatTimeString(time_t time);

private:

	/**
	 * The players that are currently racing
	 */
	std::vector<RacingPlayerInfo> m_RacingPlayers;

	/**
	 * The paths that are followed for the camera scenes
	 */
	std::u16string m_PathName;

	/**
	 * The ID of the activity for participating in this race
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
	time_t m_StartTime;

	/**
	 * Timer for tracking how long a player was alone in this race
	 */
	float m_EmptyTimer;

	bool m_SoloRacing;
};
