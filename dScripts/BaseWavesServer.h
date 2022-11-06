#pragma once
#include "ActivityManager.h"

/**
 * State for each active game
 */
struct WavesGameState {
	std::vector<LWOOBJID> players{};
	std::vector<LWOOBJID> waitingPlayers{};
	uint32_t totalSpawned = 0;
	uint32_t currentSpawned = 0;
	uint32_t waveNumber = 0;
	std::string introCelebration;
};

struct MobDefinition {
	LOT lot;
	uint32_t amountToSpawn;
	std::string spawnerName;
};

struct WaveMission {
	uint32_t time;
	uint32_t wave;
	uint32_t missionID;
};

struct Wave {
	std::vector<MobDefinition> waveMobs{};
	std::vector<uint32_t> soloMissions{};
	std::vector<uint32_t> missions{};
	std::string cinematic;
	float_t cinematicLength;
	uint32_t timeLimit = UINT32_MAX;
	bool notifyWin = false;
	uint32_t winDelay = UINT32_MAX;
};

/**
 * WaveConstants that have to be set for each game
 */
struct WaveConstants {
	uint32_t acceptedDelay = 0;
	uint32_t startDelay = 0;
	uint32_t waveTime = 0;
	uint32_t waveCompleteDelay = 0;
	std::string eventGroup;
	std::string introCelebration;
};

class BaseWavesServer : public ActivityManager {
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
	virtual WaveConstants GetConstants() { return WaveConstants(); };
	virtual std::vector<Wave> GetWaves() { return {}; };
	virtual std::vector<WaveMission> GetWaveMissions() { return {}; };
	virtual std::vector<std::string> GetSpawnerNames() { return {}; }
	WavesGameState state{};

	// Mob set default names
	std::string BaseMobSet = "baseMobSet";
	std::string RandMobSet = "randMobSet";

	// Variable names
	std::u16string WavesStartedVariable = u"wavesStarted";
	std::u16string ShowScoreboardVariable = u"Show_ScoreBoard";
	std::u16string WatchingIntroVariable = u"WatchingIntro";
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
	std::u16string UpdateCooldownVariable = u"Update_Cool_Down";
	std::u16string IsCooldownVariable = u"isCoolDown";
	std::u16string SpawnMobVariable = u"Spawn_Mob";
	std::u16string WonWaveVariable = u"Won_Wave";
	std::u16string WaveCompleteVariable = u"Wave_Complete";
	std::u16string StartTimedWaveVariable = u"Start_Timed_Wave";
	std::u16string NewWaveVariable = u"New_Wave";
	std::u16string StartCinematicVariable = u"startCinematic";
	std::u16string NumRemainingVariable = u"numRemaining";
	std::u16string StartCooldownVariable = u"Start_Cool_Down";

	// Timer names
	std::string SpawnTickTimer = "SpawnTick";
	std::string AllAcceptedDelayTimer = "AllAcceptedDelay";
	std::string AcceptedDelayTimer = "AcceptedDelay";
	std::string StartDelayTimer = "StartDelay";
	std::string ClockTickTimer = "ClockTick";
	std::string CoolDownStartTimer = "CoolDownStart";
	std::string CoolDownStopTimer = "CoolDownStop";
	std::string PlaySpawnSoundTimer = "PlaySpawnSound";
	std::string NextWaveTickTimer = "NextWaveTick";
	std::string WaveCompleteDelayTimer = "WaveCompleteDelay";
	std::string TimedWaveTimer = "TimedWave";
	std::string GameOverWinTimer = "GameOverWin";
	std::string CinematicDoneTimer = "CinematicDone";

	std::string spawnSoundGUID = "{ca36045d-89df-4e96-a317-1e152d226b69}";
private:
	void SetGameVariables(Entity* self);
	static void ResetStats(LWOOBJID player);
	void GameOver(Entity* self, bool won = false);
	void GameWon(Entity* self);
	void UpdateMissionForAllPlayers(Entity* self, uint32_t missionID);

	void StartWaves(Entity* self);
	void SpawnWave(Entity* self);
	static void SpawnNow(const std::string& spawnerName, uint32_t amount, LOT spawnLot);
	bool UpdateSpawnedEnemies(Entity* self, LWOOBJID enemyID, uint32_t score);

	bool CheckAllPlayersDead();
	void SetPlayerSpawnPoints(const LWOOBJID& specificPlayerID = LWOOBJID_EMPTY);
	void PlayerConfirmed(Entity* self);
	void PlayerAccepted(Entity* self, LWOOBJID playerID);
	void ClearSpawners();

	WaveConstants constants{};
	std::vector<Wave> waves{};
	std::vector<WaveMission> missions{};
	std::vector<std::string> spawners{};
};
