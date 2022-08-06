#pragma once
#include "CppScripts.h"

/*
--------------------------------------------------------------

--L_BOSS_SPIDER_QUEEN_ENEMY_SERVER.lua

-- Server side Spider Queen Boss fight behavior script
-- created abeechler ... 5 / 12 / 11

-- ported by Wincent01 & Max, July 2020

--------------------------------------------------------------
*/
class DestroyableComponent;
class ControllablePhysicsComponent;
class BaseCombatAIComponent;
class BossSpiderQueenEnemyServer final : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;

	void OnDie(Entity* self, Entity* killer) override;

	void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;

	void OnUpdate(Entity* self) override;

	void WithdrawSpider(Entity* self, bool withdraw);

	void SpawnSpiderWave(Entity* self, int spiderCount);

	void SpiderWaveManager(Entity* self);

	void ToggleForSpecial(Entity* self, bool state);

	void RunRainOfFire(Entity* self);

	void RainOfFireManager(Entity* self);

	void RapidFireShooterManager(Entity* self);

	void RunRapidFireShooter(Entity* self);

	float PlayAnimAndReturnTime(Entity* self, const std::u16string& animId);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	//Regular variables:
	DestroyableComponent* destroyable = nullptr;
	ControllablePhysicsComponent* controllable = nullptr;
	BaseCombatAIComponent* combat = nullptr;

	NiQuaternion originRotation;

	int m_CurrentBossStage = 0;
	int m_DeathCounter = 0;
	std::vector<int> ThresholdTable;
	bool waitForIdle = false;

	//Const variables:

	const std::vector<int> spiderWaveCntTable = { 2, 3 };	//The number of Dark Spiderling enemies to spawn per indexed wave number

	const int ROFImpactCnt = 2;			//The number of ROF impacts in each quadrant of the arena selected at random

	const LOT SpiderlingID = 16197;		//Reference obj ID for hatched Spiderlings

	const int maxSpiderEggCnt = 3;
	int hatchCounter = 0;					//Global counter mainting visibility over how many eggs we have prepped to hatch per wave
	std::vector<LWOOBJID> hatchList = {};	//Global list maintaining a record of all the eggs we have prepped to hatch for a wave

	const std::string defaultFacingZone = "Zone3Vol";	//Maintains a default facing to ensure appropriate Spider Boss positioning for teleported players
	std::vector<int> inZoneTable = {};					//Keeps track through player ID index what quadrant of the zone the player is in now
	std::vector<int> fromZoneTable = {};				//Keeps track through player ID index what quadrant of the zone the player was last in

	const float defaultAnimPause = 2.5f;							//Default period of time to pause between missing animation actions
	const std::u16string spiderJeerAnim = u"taunt";					//Animation Spider Boss plays to antagonize the player
	const std::u16string spiderROFAnim = u"attack-fire";				//Animation Spider Boss plays to telegraph the ROF attack
	const std::u16string spiderWithdrawAnim = u"withdraw";			//Animation Spider Boss plays to withdraw to the top of the mountain
	const std::u16string spiderAdvanceAnim = u"advance";				//Animation Spider Boss plays to come back down from the mountain
	const std::u16string spiderWithdrawIdle = u"idle-withdrawn";		//Animation Spider Boss plays to idle on the mountain
	const std::u16string spiderShootLeft = u"attack-shoot-left";		//Animation Spider Boss plays to RFS shoot CCW
	const std::u16string spiderShootRght = u"attack-shoot-right";		//Animation Spider Boss plays to RFS shoot CW
	const std::u16string spiderSingleShot = u"attack-fire-single";	//Animation Spider Boss plays for a single shot

	const uint32_t bossBulletSkill = 303;		//Generic Spider Boss long range attack
	const uint32_t bossSmashSkill = 322;		//Generic Spider Boss short range attack
	const uint32_t bossQueueSkill = 1568;		//Empty skill to queue for special attack timing
	const uint32_t bossLandingSkill = 1539;		//Generic Spider Boss landing attack
	const uint32_t bossSwipeSkill = 1573;		//Generic Spider Boss landing attack

	const float smashSkillLength = 3.1f;		//Time (in seconds) the boss smash skill lasts

	const float s1DelayMin = 10.0f;				//Minimum time until calling for another Rapid Fire Shot
	const float s1DelayMax = 15.0f;				//Maximum time until calling for another Rapid Fire Shot
	const float s2DelayMin = 10.0f;				//Minimum time until calling for another Rain Of Fire
	const float s2DelayMax = 15.0f;				//Maximum time until calling for another Rain Of Fire

	const unsigned int instanceZoneID = 1102;			//Zone ID for the Spider Queen fight instance
	const unsigned int instanceMissionID = 1941;			//Achievement to update for beating the instanced Boss

	//Establishes a link for the Spider to identify rapid fire targets based on zone reference
	const std::map<std::string, std::vector<std::string>> rapidFireTargetTable = {
		{"Zone1Vol", {"Zone8Targets", "Zone1Targets", "Zone2Targets"}},
		{"Zone2Vol", {"Zone1Targets", "Zone2Targets", "Zone3Targets"}},
		{"Zone3Vol", {"Zone2Targets", "Zone3Targets", "Zone4Targets"}},
		{"Zone4Vol", {"Zone3Targets", "Zone4Targets", "Zone5Targets"}},
		{"Zone5Vol", {"Zone4Targets", "Zone5Targets", "Zone6Targets"}},
		{"Zone6Vol", {"Zone5Targets", "Zone6Targets", "Zone7Targets"}},
		{"Zone7Vol", {"Zone6Targets", "Zone7Targets", "Zone8Targets"}},
		{"Zone8Vol", {"Zone7Targets", "Zone8Targets", "Zone1Targets"}}
	};

	const std::vector<std::string> ROFTargetGroupIDTable = {
		"ROF_Targets_00",
		"ROF_Targets_01",
		"ROF_Targets_02",
		"ROF_Targets_03",
		"ROF_Targets_04"
	};

	std::vector<LWOOBJID> attackTargetTable;
	std::vector<LWOOBJID> impactList;
};
