#pragma once
#include "ActivityManager.h"

enum FrakjawEnemies {
	BoneWolf = 16191,
	BlackSmith = 14007,
	Marksman = 14008,
	Commando = 14009,
	MadScientist = 16511
};

enum FrakjawLots : LOT {
	ChestLOT = 16486,
	LedgedFrakjawLOT = 16289,
	LowerFrakjawLOT = 16048,
	CounterWeightLOT = 16141
};

struct FrakjawWaveEnemy {
	LOT lot;
	uint32_t largeNumber;
	uint32_t smallNumber;
};

class NjMonastryBossInstance : public ActivityManager {
public:
	void OnStartup(Entity* self) override;
	void OnPlayerLoaded(Entity* self, Entity* player) override;
	void OnPlayerExit(Entity* self, Entity* player) override;
	void OnActivityTimerDone(Entity* self, const std::string& name) override;
private:
	void StartFight(Entity* self);
	void WaveOver(Entity* self);
	void FightOver(Entity* self);
	void SummonWave(Entity* self, Entity* frakjaw);
	void LowerFrakjaw(Entity* self, Entity* frakjaw);
	void LowerFrakjawSummon(Entity* self, Entity* frakjaw);
	void RemovePoison(Entity* self);
	static void SpawnOnNetwork(Entity* self, const LOT& toSpawn, const uint32_t& numberToSpawn, const std::string& spawnerName);
	static void TeleportPlayer(Entity* player, uint32_t position);

	// Event handlers for anything spawned by the main spawner
	void HandleLedgedFrakjawSpawned(Entity* self, Entity* ledgedFrakjaw);
	void HandleCounterWeightSpawned(Entity* self, Entity* counterWeight);
	void HandleLowerFrakjawSpawned(Entity* self, Entity* lowerFrakjaw);
	void HandleWaveEnemySpawned(Entity* self, Entity* waveEnemy);
	void HandleWaveEnemyDied(Entity* self, Entity* waveEnemy);
	void HandleLowerFrakjawHit(Entity* self, Entity* lowerFrakjaw, Entity* attacker);
	void HandleLowerFrakjawDied(Entity* self, Entity* lowerFrakjaw);

	const std::vector<std::vector<FrakjawWaveEnemy>> m_Waves = {
		// Wave 1
		{
			{ FrakjawEnemies::Marksman, 2, 1},
			{ FrakjawEnemies::BlackSmith, 4, 3},
			{ FrakjawEnemies::Commando, 2, 1},
			{ FrakjawEnemies::MadScientist, 1, 0},
		},

		// Wave 2
		{
			{ FrakjawEnemies::BoneWolf, 1, 0},
			{ FrakjawEnemies::BlackSmith, 2, 2},
			{ FrakjawEnemies::Marksman, 2, 1},
			{ FrakjawEnemies::MadScientist, 1, 1},
		},

		// Wave 3
		{
			{ FrakjawEnemies::BoneWolf, 2, 1},
			{ FrakjawEnemies::Marksman, 2, 1},
			{ FrakjawEnemies::Commando, 2, 2},
			{ FrakjawEnemies::MadScientist, 1, 0},
		},

		// Wave 4
		{
			{ FrakjawEnemies::BlackSmith, 2, 2},
			{ FrakjawEnemies::BoneWolf, 1, 1},
			{ FrakjawEnemies::Commando, 3, 1},
			{ FrakjawEnemies::Marksman, 2, 0},
		},

		// Wave 5
		{
			{ FrakjawEnemies::MadScientist, 1, 0},
			{ FrakjawEnemies::BoneWolf, 2, 0},
			{ FrakjawEnemies::Commando, 3, 0},
			{ FrakjawEnemies::Marksman, 2, 0},
		}
	};

	const int32_t PoisonBuff = 60;

	// Variables
	const std::u16string TotalPlayersLoadedVariable = u"TotalPlayersLoaded";
	const std::u16string LargeTeamVariable = u"LargeTeam";
	const std::u16string FightStartedVariable = u"FightStarted";
	const std::u16string LedgeFrakjawVariable = u"LedgeFrakjaw";
	const std::u16string LowerFrakjawVariable = u"LowerFrakjaw";
	const std::u16string WaveNumberVariable = u"WaveNumber";
	const std::u16string OnLastWaveVarbiale = u"OnLastWave";
	const std::u16string TrashMobsAliveVariable = u"TrashMobsAlive";
	const std::u16string TotalAliveInWaveVariable = u"TotalAliveInWave";

	// Timers
	const char TimerSplitChar = '+';
	const std::string WaitingForPlayersTimer = "WaitingForPlayers";
	const std::string SpawnWaveTimer = "SpawnWave";
	const std::string SpawnNextWaveTimer = "SpawnNextWave";
	const std::string UnstunTimer = "Unstun+";
	const std::string FrakjawSpawnInTimer = "LowerFrakjawSpawnIn+";
	const std::string WaveOverTimer = "WaveOverTimer";
	const std::string FightOverTimer = "FightOver";
	const std::string LowerFrakjawCamTimer = "StartLowerFrakjawCam";
	const std::string SpawnCounterWeightTimer = "SpawnQB";
	const std::string SpawnRailTimer = "SpawnRailQB";
	const std::string SpawnLowerFrakjawTimer = "SpawnLowerFrakjaw";

	// Groups
	const std::string ChestSpawnpointGroup = "ChestSpawnPoint";

	// Spawner network names
	const std::string LedgeFrakjawSpawner = "LedgeFrakjaw";
	const std::string LowerFrakjawSpawner = "LowerFrakjaw";
	const std::string BaseEnemiesSpawner = "EnemySpawnPoints_";
	const std::string CounterweightSpawner = "Counterweights";
	const std::string FireRailSpawner = "FireRailActivatorQB";
	const std::string ExtraRocks = "ExtraRocks";

	// Cinematics
	const std::string LedgeFrakSummon = "FrakjawSummoning";
	const std::string BaseCounterweightQB = "CounterweightQB";
	const std::string BaseCounterweightSpawn = "CWQBSpawn";
	const std::string BottomFrakSummon = "BottomFrakjawSummoning";
	const std::string BottomFrakSpawn = "BottomFrakjawSpawning";
	const std::string TreasureChestSpawning = "TreasureChestSpawning";
	const std::string FireRailSpawn = "RailQBSpawn";

	// Notifications
	const std::u16string StopMusicNotification = u"StopMusic";
	const std::u16string StartMusicNotification = u"StartMusic";
	const std::u16string FlashMusicNotification = u"FlashMusic";
	const std::u16string PlayCinematicNotification = u"PlayCinematic";
	const std::u16string EndCinematicNotification = u"EndCinematic";

	// Animations
	const std::u16string SummonAnimation = u"summon";
	const std::u16string TeleportOutAnimation = u"teleport-out";
	const std::u16string TeleportInAnimation = u"teleport-in";
	const std::u16string StunnedAnimation = u"stunned";

	// Audio cues
	const std::string AudioWaveAudio = "Monastery_Frakjaw_Battle_";
	const std::string BattleOverAudio = "Monastery_Frakjaw_Battle_Win";
	const std::string CounterSmashAudio = "{d76d7b9d-9dc2-4e52-a315-69b25ef521ca}";
};
