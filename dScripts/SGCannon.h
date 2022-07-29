#pragma once
#include <regex>
#include "ActivityManager.h"

struct SGEnemy {
	std::vector<std::string> spawnPaths{};
	LOT lot;
	float_t minSpawnTime;
	float_t maxSpawnTime;
	bool respawns;
	float_t minRespawnTime;
	float_t maxRespawnTime;
	float_t initialSpeed;
	int32_t score;
	bool changeSpeedAtWaypoint;
	float_t speedChangeChance;
	float_t minSpeed;
	float_t maxSpeed;
	bool isMovingPlatform;
	bool despawnOnLastWaypoint;
};

struct SGConstants {
	Vector3 playerStartPosition;
	Quaternion playerStartRotation;
	LOT cannonLot;
	uint32_t impactSkillID;
	LOT projectileLot;
	Vector3 playerOffset;
	uint32_t rewardModelMatrix;
	float_t cannonVelocity;
	float_t cannonMinDistance;
	float_t cannonRefireRate;
	Vector3 cannonBarrelOffset;
	LOT cannonSuperchargedProjectileLot;
	LOT cannonProjectileLot;
	uint32_t cannonSuperChargeSkill;
	uint32_t cannonSkill;
	int32_t cannonTimeout;
	float_t cannonFOV;
	bool useLeaderboards;
	uint32_t streakModifier;
	uint32_t chargedTime;
	uint32_t chargedPoints;
	std::string rewardModelGroup;
	uint32_t activityID;
	uint32_t scoreReward1;
	uint32_t scoreLootMatrix1;
	uint32_t scoreReward2;
	uint32_t scoreLootMatrix2;
	uint32_t scoreReward3;
	uint32_t scoreLootMatrix3;
	uint32_t scoreReward4;
	uint32_t scoreLootMatrix4;
	uint32_t scoreReward5;
	uint32_t scoreLootMatrix5;
	float_t firstWaveStartTime;
	float_t inBetweenWavePause;
};

class SGCannon : public ActivityManager {
public:
	void OnStartup(Entity* self) override;
	void OnPlayerLoaded(Entity* self, Entity* player) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
	void OnActivityStateChangeRequest(Entity* self, LWOOBJID senderID, int32_t value1,
		int32_t value2, const std::u16string& stringValue) override;
	void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier,
		const std::u16string& userData) override;
	void OnActivityTimerDone(Entity* self, const std::string& name) override;
	void OnActivityTimerUpdate(Entity* self, const std::string& name, float_t timeRemaining, float_t elapsedTime) override;
private:
	static std::vector<std::vector<SGEnemy>> GetWaves();
	static SGConstants GetConstants();
	void ResetVars(Entity* self);
	void StartGame(Entity* self);
	void DoGameStartup(Entity* self);
	void SpawnNewModel(Entity* self);
	void TimerToggle(Entity* self, bool start = false);
	void SpawnObject(Entity* self, const SGEnemy& toSpawn, bool spawnNow = false);
	void StartChargedCannon(Entity* self, uint32_t optionalTime = 0);
	void ToggleSuperCharge(Entity* self, bool enable);
	void RecordPlayerScore(Entity* self);
	void PlaySceneAnimation(Entity* self, const std::u16string& animationName, bool onCannon, bool onPlayer, float_t priority);
	static void RemovePlayer(LWOOBJID playerID);
	void PauseChargeCannon(Entity* self);
	void StopGame(Entity* self, bool cancel = false);
	void RegisterHit(Entity* self, Entity* target, const std::string& timerName);
	void UpdateStreak(Entity* self);
	float_t GetCurrentBonus(Entity* self);

	LOT m_CannonLot = 1864;
	std::u16string PlayerIDVariable = u"PlayerID";
	std::u16string HideScoreBoardVariable = u"HideScoreBoard";
	std::u16string ReSetSuperChargeVariable = u"ReSetSuperCharge";
	std::u16string ShowLoadingUI = u"showLoadingUI";
	std::u16string SpawnNumberVariable = u"SpawnNum";
	std::u16string CurrentSpawnNumberVariable = u"CurSpawnNum";
	std::u16string ThisWaveVariable = u"ThisWave";
	std::u16string GameScoreVariable = u"GameScore";
	std::u16string GameTimeVariable = u"GameTime";
	std::u16string GameStartedVariable = u"GameStarted";
	std::u16string ShotsFiredVariable = u"ShotsFired";
	std::u16string MaxStreakVariable = u"MaxStreak";
	std::u16string MissesVariable = u"Misses";
	std::u16string CurrentStreakVariable = u"CurrentStreak";
	std::u16string CurrentSuperChargedTimeVariable = u"CurrentSuperChargedTime";
	std::u16string StreakBonusVariable = u"StreakBonus";
	std::u16string LastSuperTotalVariable = u"LastSuperTotal";
	std::u16string CurrentRewardVariable = u"CurrentReward";
	std::u16string RewardsVariable = u"Rewards";
	std::u16string TotalScoreVariable = u"TotalScore";
	std::u16string InitVariable = u"Init";
	std::u16string ImpactSkillVariale = u"ImpactSkill";
	std::u16string PlayerScoresVariable = u"PlayerScores";
	std::u16string InitialVelocityVariable = u"InitialVelocity";
	std::u16string ValidActorsVariable = u"ValidActors";
	std::u16string ValidEffectsVariable = u"ValidEffects";
	std::u16string SuperChargeActiveVariable = u"SuperChargeActive";
	std::u16string MatrixVariable = u"Matrix";
	std::u16string TimeLimitVariable = u"game_timelimit";
	std::u16string AudioStartIntroVariable = u"Audio_Start_Intro";
	std::u16string ClearVariable = u"Clear";
	std::u16string FirstTimeDoneVariable = u"FirstTimeDone";
	std::u16string RewardAddedVariable = u"rewardAdded";
	std::u16string SuperChargePausedVariable = u"Super_Charge_Paused";
	std::u16string WaveStatusVariable = u"WaveStatus";
	std::u16string CountVariable = u"count";
	std::u16string StopVariable = u"Stop";
	std::u16string ActiveSpawnsVariable = u"ActiveSpawns";
	std::u16string SpawnedObjects = u"SpawnedObjects";
	std::u16string WaveNumVariable = u"wave.waveNum";
	std::u16string WaveStrVariable = u"wave.waveStr";
	std::u16string ChargeCountingVariable = u"charge_counting";
	std::u16string SuperChargeBarVariable = u"SuperChargeBar";
	std::u16string NumberOfChargesVariable = u"NumberOfCharges";
	std::u16string CannonBallSkillIDVariable = u"cbskill";
	std::u16string HideSuperChargeVariable = u"HideSuper";
	std::u16string AudioFinalWaveDoneVariable = u"Audio_Final_Wave_Done";

	std::string SpawnWaveTimer = "SpawnWave";
	std::string EndWaveTimer = "EndWave";
	std::string GameOverTimer = "GameOver";
	std::string DoSpawnTimer = "DoSpawn";
	std::string EndGameBufferTimer = "endGameBuffer";
	std::string SuperChargeTimer = "SuperChargeTimer";

	std::string ModelToBuildEvent = "modelToBuild";

	std::regex DoSpawnRegex = std::regex("\\d*");
	SGConstants constants{};
	std::vector<std::vector<SGEnemy>> m_Waves{};
};
