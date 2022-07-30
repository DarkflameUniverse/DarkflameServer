#pragma once
#include "ActivityManager.h"

/**
 * State for each active game
 */
struct GameState {
	std::vector<LWOOBJID> players{};
	std::vector<LWOOBJID> waitingPlayers{};
	uint32_t totalSpawned = 0;
	uint32_t rewardTick = 0;
	uint32_t waveNumber = 1;
};

/**
 * Constants that have to be set for each game
 */
struct Constants {
	uint32_t acceptedDelay = 0;
	uint32_t startDelay = 0;
	uint32_t waveTime = 0;
	uint32_t rewardInterval = 0;
	uint32_t coolDownTime = 0;
	uint32_t mobSet2Wave = 0;
	uint32_t mobSet3Wave = 0;
	uint32_t unlockNetwork3 = 0;
	uint32_t lotPhase = 0;
	bool useMobLots = false;
	std::vector<uint32_t> baseMobsStartTier{};
	std::vector<uint32_t> randMobsStartTier{};
};

/**
 * The lots of the mobs to spawn along with amounts to spawn for each wave
 */
struct MobSets {
	std::map<std::string, std::vector<LOT>> mobLots{};
	std::map<std::string, std::vector<std::vector<std::vector<uint32_t>>>> sets{};
};

struct SpawnerNetwork {
	std::vector<std::string> names{};
	std::string number;
	bool isLocked = false;
	bool isActive = false;
};

struct SpawnerNetworkCollection {
	std::string mobSetName;
	std::vector<SpawnerNetwork> networks{};
};

struct SpawnerNetworks {
	SpawnerNetworkCollection baseNetworks{};
	SpawnerNetworkCollection randomNetworks{};
	SpawnerNetworkCollection rewardNetworks{};
	SpawnerNetworkCollection smashNetworks{};
};

class BaseSurvivalServer : public ActivityManager {
public:
	void OnStartup(Entity* self) override { SetGameVariables(self); BaseStartup(self); };
	void OnPlayerLoaded(Entity* self, Entity* player) override { BasePlayerLoaded(self, player); };
	void OnPlayerExit(Entity* self, Entity* player) override { BasePlayerExit(self, player); };
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override {
		BaseFireEvent(self, sender, args, param1, param2, param3);
	};
	void OnPlayerDied(Entity* self, Entity* player) override { BasePlayerDied(self, player); };
	void OnPlayerResurrected(Entity* self, Entity* player) override { BasePlayerResurrected(self, player); };
	void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier,
		const std::u16string& userData) override {
		BaseMessageBoxResponse(self, sender, button, identifier, userData);
	};

	void BasePlayerLoaded(Entity* self, Entity* player);
	void BaseStartup(Entity* self);
	void BasePlayerExit(Entity* self, Entity* player);
	void BaseFireEvent(Entity* self, Entity* sender, const std::string& args, int32_t param1, int32_t param2,
		int32_t param3);
	void BasePlayerDied(Entity* self, Entity* player);
	void BasePlayerResurrected(Entity* self, Entity* player);
	void BaseMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData);

	void OnActivityTimerDone(Entity* self, const std::string& name) override;
	void OnActivityTimerUpdate(Entity* self, const std::string& name, float_t remainingTime, float_t elapsedTime) override;
protected:
	virtual Constants GetConstants() { return Constants(); };
	virtual MobSets GetMobSets() { return MobSets(); };
	virtual SpawnerNetworks GetSpawnerNetworks() { return SpawnerNetworks(); };
	virtual std::map<uint32_t, uint32_t> GetMissionsToUpdate() { return {}; };
	GameState state{};

	// Mob set default names
	std::string BaseMobSet = "baseMobSet";
	std::string RandMobSet = "randMobSet";

	// Variable names
	std::u16string WavesStartedVariable = u"wavesStarted";
	std::u16string ShowScoreboardVariable = u"Show_ScoreBoard";
	std::u16string ClearScoreboardVariable = u"Clear_Scoreboard";
	std::u16string DefinePlayerToUIVariable = u"Define_Player_To_UI";
	std::u16string UpdateScoreboardPlayersVariable = u"Update_ScoreBoard_Players.";
	std::u16string PlayerConfirmVariable = u"PlayerConfirm_ScoreBoard.";
	std::u16string PlayersAcceptedVariable = u"playersAccepted";
	std::u16string PlayersReadyVariable = u"playersReady";
	std::u16string BaseMobSetIndexVariable = u"baseMobSetNum";
	std::u16string RandMobSetIndexVariable = u"randMobSetNum";
	std::u16string AcceptedDelayStartedVariable = u"AcceptedDelayStarted";
	std::u16string NumberOfPlayersVariable = u"NumberOfPlayers";
	std::u16string FirstTimeDoneVariable = u"firstTimeDone";
	std::u16string MissionTypeVariable = u"missionType";
	std::u16string StartWaveMessageVariable = u"Start_Wave_Message";
	std::u16string ExitWavesVariable = u"Exit_waves";
	std::u16string UpdateDefaultStartTimerVariable = u"Update_Default_Start_Timer";
	std::u16string UpdateTimerVariable = u"Update_Timer";
	std::u16string IsCooldownVariable = u"isCoolDown";
	std::u16string SpawnMobVariable = u"Spawn_Mob";

	// Timer names
	std::string SpawnTickTimer = "SpawnTick";
	std::string AllAcceptedDelayTimer = "AllAcceptedDelay";
	std::string AcceptedDelayTimer = "AcceptedDelay";
	std::string StartDelayTimer = "StartDelay";
	std::string ClockTickTimer = "ClockTick";
	std::string CoolDownStartTimer = "CoolDownStart";
	std::string CoolDownStopTimer = "CoolDownStop";
	std::string PlaySpawnSoundTimer = "PlaySpawnSound";

	std::string spawnSoundGUID = "{ca36045d-89df-4e96-a317-1e152d226b69}";
private:
	void SetGameVariables(Entity* self);
	static void ResetStats(LWOOBJID player);
	void GameOver(Entity* self);

	void StartWaves(Entity* self);
	static void ActivateSpawnerNetwork(SpawnerNetworkCollection& spawnerNetworkCollection);
	void SpawnerReset(SpawnerNetworkCollection& spawnerNetworkCollection, bool hardReset = true);
	void SpawnerUpdate(Entity* self, SpawnerNetworkCollection& spawnerNetworkCollection, uint32_t amount = 0);
	void SpawnMobs(Entity* self);
	static SpawnerNetwork GetRandomSpawner(SpawnerNetworkCollection& spawnerNetworkCollection);
	std::vector<uint32_t> GetRandomMobSet(SpawnerNetworkCollection& spawnerNetworkCollection, uint32_t setNumber);

	static void SpawnNow(Spawner* spawner, uint32_t amount);

	bool CheckAllPlayersDead();
	void SetPlayerSpawnPoints();
	void UpdateMobLots(SpawnerNetworkCollection& spawnerNetworkCollection);
	void PlayerConfirmed(Entity* self);
	void PlayerAccepted(Entity* self, LWOOBJID playerID);

	Constants constants{};
	MobSets mobSets{};
	SpawnerNetworks spawnerNetworks{};
	std::map<uint32_t, uint32_t> missionsToUpdate{};
};
