#include "BossSpiderQueenEnemyServer.h"

#include "GeneralUtils.h"

#include "MissionComponent.h"
#include "EntityManager.h"
#include "Entity.h"
#include "dZoneManager.h"

#include "DestroyableComponent.h"
#include "ControllablePhysicsComponent.h"
#include "BaseCombatAIComponent.h"

#include "GameMessages.h"
#include "SkillComponent.h"
#include "eReplicaComponentType.h"
#include "RenderComponent.h"

#include <vector>

//----------------------------------------------------------------
//--On Startup, process necessary AI events
//----------------------------------------------------------------
void BossSpiderQueenEnemyServer::OnStartup(Entity* self) {
	// Make immune to stuns
	//self:SetStunImmunity{ StateChangeType = "PUSH", bImmuneToStunAttack = true, bImmuneToStunMove = true, bImmuneToStunTurn = true, bImmuneToStunUseItem = true, bImmuneToStunEquip = true, bImmuneToStunInteract = true, bImmuneToStunJump = true }

	// Make immune to knockbacks and pulls
	//self:SetStatusImmunity{ StateChangeType = "PUSH", bImmuneToPullToPoint = true, bImmuneToKnockback = true, bImmuneToInterrupt = true }

	//Get our components:
	destroyable = static_cast<DestroyableComponent*>(self->GetComponent(eReplicaComponentType::DESTROYABLE));
	controllable = static_cast<ControllablePhysicsComponent*>(self->GetComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS));
	combat = static_cast<BaseCombatAIComponent*>(self->GetComponent(eReplicaComponentType::BASE_COMBAT_AI));

	if (!destroyable || !controllable) return;

	// Determine Spider Boss health transition thresholds
	int spiderBossHealth = destroyable->GetMaxHealth();
	int transitionTickHealth = spiderBossHealth / 3;

	int Stage2HealthThreshold = spiderBossHealth - transitionTickHealth;
	int Stage3HealthThreshold = spiderBossHealth - (2 * transitionTickHealth);
	ThresholdTable = { Stage2HealthThreshold, Stage3HealthThreshold };

	originRotation = controllable->GetRotation();
	combat->SetStunImmune(true);

	m_CurrentBossStage = 1;

	// Obtain faction and collision group to save for subsequent resets
}

void BossSpiderQueenEnemyServer::OnDie(Entity* self, Entity* killer) {
	if (Game::zoneManager->GetZoneID().GetMapID() == instanceZoneID) {
		auto* missionComponent = killer->GetComponent<MissionComponent>();
		if (missionComponent == nullptr)
			return;

		missionComponent->CompleteMission(instanceMissionID);
	}

	// There is suppose to be a 0.1 second delay here but that may be admitted?
	auto* controller = Game::entityManager->GetZoneControlEntity();

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"SetColGroup", 10, 0, 0, "", UNASSIGNED_SYSTEM_ADDRESS);

	self->SetPosition({ 10000, 0, 10000 });

	Game::entityManager->SerializeEntity(self);

	controller->OnFireEventServerSide(self, "ClearProperty");
}

void BossSpiderQueenEnemyServer::WithdrawSpider(Entity* self, const bool withdraw) {
	const auto withdrawn = self->GetBoolean(u"isWithdrawn");

	if (withdrawn == withdraw) {
		return;
	}

	if (withdraw) {
		//Move spider away from battle zone
		// Disabled because we cant option the reset collition group right now
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"SetColGroup", 10, 0, 0, "", UNASSIGNED_SYSTEM_ADDRESS);

		//First rotate for anim
		NiQuaternion rot = NiQuaternion::IDENTITY;

		controllable->SetStatic(false);

		controllable->SetRotation(rot);

		controllable->SetStatic(true);

		controllable->SetDirtyPosition(true);

		rot = controllable->GetRotation();

		Game::entityManager->SerializeEntity(self);

		auto* baseCombatAi = self->GetComponent<BaseCombatAIComponent>();

		baseCombatAi->SetDisabled(true);

		float animTime = PlayAnimAndReturnTime(self, spiderWithdrawAnim);
		float withdrawTime = animTime - 0.25f;

		combat->SetStunImmune(false);
		combat->Stun(withdrawTime + 6.0f);
		combat->SetStunImmune(true);

		//TODO: Set faction to -1 and set immunity
		destroyable->SetFaction(-1);
		destroyable->SetIsImmune(true);
		Game::entityManager->SerializeEntity(self);

		self->AddTimer("WithdrawComplete", withdrawTime + 1.0f);
		waitForIdle = true;
	} else {
		controllable->SetStatic(false);

		//Cancel all remaining timers for say idle anims:
		self->CancelAllTimers();

		auto* baseCombatAi = self->GetComponent<BaseCombatAIComponent>();

		baseCombatAi->SetDisabled(false);

		// Move the Spider to its ground location
		// preparing its stage attacks, and removing invulnerability
		//destroyable->SetIsImmune(false);

		// Run the advance animation and prepare a timer for resuming AI
		float animTime = PlayAnimAndReturnTime(self, spiderAdvanceAnim);
		animTime += 1.f;

		float attackPause = animTime - 0.4f;

		destroyable->SetFaction(4);
		destroyable->SetIsImmune(false);

		//Advance stage
		m_CurrentBossStage++;

		//Reset the current wave death counter
		m_DeathCounter = 0;

		Game::entityManager->SerializeEntity(self);

		// Prepare a timer for post leap attack
		self->AddTimer("AdvanceAttack", attackPause);

		// Prepare a timer for post leap
		self->AddTimer("AdvanceComplete", animTime);
	}

	self->SetBoolean(u"isWithdrawn", withdraw);
}

void BossSpiderQueenEnemyServer::SpawnSpiderWave(Entity* self, int spiderCount) {
	// The Spider Queen Boss is withdrawing and requesting the spawn
	// of a hatchling wave

	// Clamp invalid Spiderling number requests to the maximum amount of eggs available
	if ((spiderCount > maxSpiderEggCnt) || (spiderCount < 0))
		spiderCount = maxSpiderEggCnt;

	// Reset our wave manager reference variables
	hatchCounter = spiderCount;
	hatchList = {};

	// Run the wave manager
	SpiderWaveManager(self);
}

void BossSpiderQueenEnemyServer::SpiderWaveManager(Entity* self) {
	auto SpiderEggNetworkID = self->GetI64(u"SpiderEggNetworkID");

	std::vector<LWOOBJID> spiderEggs{};

	auto spooders = Game::entityManager->GetEntitiesInGroup("EGG");
	for (auto spodder : spooders) {
		spiderEggs.push_back(spodder->GetObjectID());
	}

	// Select a number of random spider eggs from the list equal to the
	// current number needed to complete the current wave
	if (!spiderEggs.empty()) {
		for (int i = 0; i < hatchCounter; i++) {
			// Select a random spider egg
			auto randomEggLoc = GeneralUtils::GenerateRandomNumber<int>(0, spiderEggs.size() - 1);
			auto randomEgg = spiderEggs[randomEggLoc];

			//Just a quick check to try and prevent dupes:
			for (auto en : hatchList) {
				if (en == randomEgg) {
					randomEggLoc++;
					randomEgg = spiderEggs[randomEggLoc];
				}
			}

			if (randomEgg) {
				auto* eggEntity = Game::entityManager->GetEntity(randomEgg);

				if (eggEntity == nullptr) {
					continue;
				}

				// Prep the selected spider egg
				eggEntity->OnFireEventServerSide(self, "prepEgg");

				// Add the prepped egg to our hatchList
				hatchList.push_back(eggEntity->GetObjectID());

				// Decrement the hatchCounter
				hatchCounter = hatchCounter - 1;
			}

			// Remove it from our spider egg list
			spiderEggs[randomEggLoc] = LWOOBJID_EMPTY;

			if (spiderEggs.size() <= 0 || (hatchCounter <= 0)) {
				break;
			}
		}
	}

	if (hatchCounter > 0) {
		// We still have more eggs to hatch, poll the SpiderWaveManager again
		self->AddTimer("PollSpiderWaveManager", 1.0f);

	} else {
		// We have successfully readied a full wave
		// initiate hatching!
		for (auto egg : hatchList) {
			auto* eggEntity = Game::entityManager->GetEntity(egg);

			if (eggEntity == nullptr) {
				continue;
			}

			eggEntity->OnFireEventServerSide(self, "hatchEgg");

			auto time = PlayAnimAndReturnTime(self, spiderWithdrawIdle);
			combat->SetStunImmune(false);
			combat->Stun(time += 6.0f);
			combat->SetStunImmune(true);

			self->AddTimer("checkForSpiders", 6.0f);

		}

		hatchList.clear();

	}

}

void BossSpiderQueenEnemyServer::ToggleForSpecial(Entity* self, const bool state) {
	self->SetBoolean(u"stoppedFlag", state);

	combat->SetDisabled(state);
}

void BossSpiderQueenEnemyServer::RunRainOfFire(Entity* self) {
	if (self->GetBoolean(u"stoppedFlag")) {
		self->AddTimer("ROF", GeneralUtils::GenerateRandomNumber<float>(10, 20));

		return;
	}

	ToggleForSpecial(self, true);

	impactList.clear();

	auto index = 0u;
	for (const auto& rofGroup : ROFTargetGroupIDTable) {
		const auto spawners = Game::zoneManager->GetSpawnersInGroup(rofGroup);

		std::vector<LWOOBJID> spawned;

		for (auto* spawner : spawners) {
			for (const auto* node : spawner->m_Info.nodes) {
				spawned.insert(spawned.end(), node->entities.begin(), node->entities.end());
			}
		}

		if (index == 0) {
			impactList.insert(impactList.end(), spawned.begin(), spawned.end());
		} else {
			const auto randomIndex = GeneralUtils::GenerateRandomNumber<int32_t>(0, spawned.size() - 1);

			impactList.push_back(spawned[randomIndex]);
		}

		index++;
	}

	const auto animTime = PlayAnimAndReturnTime(self, spiderROFAnim);

	self->AddTimer("StartROF", animTime);
}

void BossSpiderQueenEnemyServer::RainOfFireManager(Entity* self) {
	if (!impactList.empty()) {
		auto* entity = Game::entityManager->GetEntity(impactList[0]);

		impactList.erase(impactList.begin());

		if (entity == nullptr) {
			Game::logger->Log("BossSpiderQueenEnemyServer", "Failed to find impact!");

			return;
		}

		auto* skillComponent = entity->GetComponent<SkillComponent>();

		if (skillComponent == nullptr) {
			Game::logger->Log("BossSpiderQueenEnemyServer", "Failed to find impact skill component!");

			return;
		}

		skillComponent->CalculateBehavior(1376, 32168, LWOOBJID_EMPTY, true);

		self->AddTimer("PollROFManager", 0.5f);

		return;
	}

	ToggleForSpecial(self, false);

	self->AddTimer("ROF", GeneralUtils::GenerateRandomNumber<float>(20, 40));
}

void BossSpiderQueenEnemyServer::RapidFireShooterManager(Entity* self) {
	if (attackTargetTable.empty()) {
		const auto animationTime = PlayAnimAndReturnTime(self, spiderJeerAnim);

		self->AddTimer("RFSTauntComplete", animationTime);

		ToggleForSpecial(self, false);

		return;
	}

	const auto target = attackTargetTable[0];

	auto* skillComponent = self->GetComponent<SkillComponent>();

	skillComponent->CalculateBehavior(1394, 32612, target, true);

	attackTargetTable.erase(attackTargetTable.begin());

	self->AddTimer("PollRFSManager", 0.3f);
}

void BossSpiderQueenEnemyServer::RunRapidFireShooter(Entity* self) {
	const auto targets = self->GetTargetsInPhantom();

	if (self->GetBoolean(u"stoppedFlag")) {
		self->AddTimer("RFS", GeneralUtils::GenerateRandomNumber<float>(5, 10));

		return;
	}

	if (targets.empty()) {
		Game::logger->Log("BossSpiderQueenEnemyServer", "Failed to find RFS targets");

		self->AddTimer("RFS", GeneralUtils::GenerateRandomNumber<float>(5, 10));

		return;
	}

	ToggleForSpecial(self, true);

	const auto randomTarget = GeneralUtils::GenerateRandomNumber<int32_t>(0, targets.size() - 1);

	auto attackFocus = targets[randomTarget];

	attackTargetTable.push_back(attackFocus);

	auto* skillComponent = self->GetComponent<SkillComponent>();

	skillComponent->CalculateBehavior(1480, 36652, attackFocus, true);

	RapidFireShooterManager(self);

	PlayAnimAndReturnTime(self, spiderSingleShot);

	self->AddTimer("RFS", GeneralUtils::GenerateRandomNumber<float>(10, 15));
}

void BossSpiderQueenEnemyServer::OnTimerDone(Entity* self, const std::string timerName) {
	if (timerName == "PollSpiderWaveManager") {
		//Call the manager again to attempt to finish prepping a Spiderling wave
		//Run the wave manager
		SpiderWaveManager(self);
	} else if (timerName == "disableWaitForIdle") { waitForIdle = false; } else if (timerName == "checkForSpiders") {
		//Don't do anything if we ain't withdrawn:
		const auto withdrawn = self->GetBoolean(u"isWithdrawn");
		if (!withdrawn) return;

		NiQuaternion rot = NiQuaternion::IDENTITY;

		//First rotate for anim
		controllable->SetStatic(false);
		controllable->SetRotation(rot);
		controllable->SetStatic(true);
		Game::entityManager->SerializeEntity(self);

		//Play the Spider Boss' mountain idle anim
		auto time = PlayAnimAndReturnTime(self, spiderWithdrawIdle);
		combat->SetStunImmune(false);
		combat->Stun(time);
		combat->SetStunImmune(true);

		rot = controllable->GetRotation();

		//If there are still baby spiders, don't do anyhting either
		const auto spiders = Game::entityManager->GetEntitiesInGroup("BabySpider");
		if (spiders.size() > 0)
			self->AddTimer("checkForSpiders", time);
		else
			WithdrawSpider(self, false);
	} else if (timerName == "PollROFManager") {
		//Call the manager again to attempt to initiate an impact on another random location
		//Run the ROF Manager
		RainOfFireManager(self);

	} else if (timerName == "PollRFSManager") {
		//Call the manager again to attempt to initiate a rapid fire shot at the next sequential target
		//Run the ROF Manager
		RapidFireShooterManager(self);

	} else if (timerName == "StartROF") {
		//Re-enable Spider Boss
		//ToggleForSpecial(self, false);

		RainOfFireManager(self);

	} else if (timerName == "PollSpiderSkillManager") {
		//Call the skill manager again to attempt to run the current Spider Boss
		//stage's special attack again
		//SpiderSkillManager(self, true);
		PlayAnimAndReturnTime(self, spiderJeerAnim);

	} else if (timerName == "RFS") {
		RunRapidFireShooter(self);
	} else if (timerName == "ROF") {
		RunRainOfFire(self);
	} else if (timerName == "RFSTauntComplete") {
		//Determine an appropriate random time to check our manager again
	   // local spiderCooldownDelay = math.random(s1DelayMin, s1DelayMax)

		//Set a timer based on our random cooldown determination
		//to pulse the SpiderSkillManager again

		//GAMEOBJ:GetTimer():AddTimerWithCancel(spiderCooldownDelay, "PollSpiderSkillManager", self)

		//Re-enable Spider Boss
		//ToggleForSpecial(self, false);

	} else if (timerName == "WithdrawComplete") {
		//Play the Spider Boss' mountain idle anim
		PlayAnimAndReturnTime(self, spiderWithdrawIdle);

		//The Spider Boss has retreated, hatch a wave!
		int currentStage = m_CurrentBossStage;

		//Prepare a Spiderling wave and initiate egg hatch events
		//self->SetVar(u"SpiderWaveCount", )

		//TODO: Actually spawn the spiders here
		hatchCounter = 2;
		if (currentStage > 1) hatchCounter++;

		SpawnSpiderWave(self, spiderWaveCntTable[currentStage - 1]);

	} else if (timerName == "AdvanceAttack") {
		//TODO: Can we even do knockbacks yet? @Wincent01
		// Yes ^

		//Fire the melee smash skill to throw players back
		/*local landingTarget = self:GetVar("LandingTarget") or false

		if((landingTarget) and (landingTarget:Exists())) {
			local advSmashFlag = landingTarget:CastSkill{skillID = bossLandingSkill}
			landingTarget:PlayEmbeddedEffectOnAllClientsNearObject{radius = 100, fromObjectID = landingTarget, effectName = "camshake-bridge"}
		}*/

		auto landingTarget = self->GetI64(u"LandingTarget");
		auto landingEntity = Game::entityManager->GetEntity(landingTarget);

		auto* skillComponent = self->GetComponent<SkillComponent>();

		if (skillComponent != nullptr) {
			skillComponent->CalculateBehavior(bossLandingSkill, 37739, LWOOBJID_EMPTY);
		}

		if (landingEntity) {
			auto* landingSkill = landingEntity->GetComponent<SkillComponent>();

			if (landingSkill != nullptr) {
				landingSkill->CalculateBehavior(bossLandingSkill, 37739, LWOOBJID_EMPTY, true);
			}
		}

		GameMessages::SendPlayEmbeddedEffectOnAllClientsNearObject(self, u"camshake-bridge", self->GetObjectID(), 100.0f);

	} else if (timerName == "AdvanceComplete") {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"SetColGroup", 11, 0, 0, "", UNASSIGNED_SYSTEM_ADDRESS);

		//Wind up, telegraphing next round
		float animTime = PlayAnimAndReturnTime(self, spiderJeerAnim);
		self->AddTimer("AdvanceTauntComplete", animTime);

	} else if (timerName == "AdvanceTauntComplete") {

		//Declare a default special Spider Boss skill cooldown
		int spiderCooldownDelay = 10;

		if (m_CurrentBossStage == 2) {
			spiderCooldownDelay = GeneralUtils::GenerateRandomNumber<int>(s1DelayMin, s1DelayMax);
		} else if (m_CurrentBossStage == 3) {
			spiderCooldownDelay = GeneralUtils::GenerateRandomNumber<int>(s2DelayMin, s2DelayMax);
		}

		//Set a timer based on our random cooldown determination
		//to pulse the SpiderSkillManager
		self->AddTimer("PollSpiderSkillManager", spiderCooldownDelay);

		//Remove current status immunity
		/*self:SetStatusImmunity{ StateChangeType = "POP", bImmuneToSpeed = true, bImmuneToBasicAttack = true, bImmuneToDOT = true}

		self:SetStunned{StateChangeType = "POP",
					bCantMove = true,
					bCantJump = true,
					bCantTurn = true,
					bCantAttack = true,
					bCantUseItem = true,
					bCantEquip = true,
					bCantInteract = true,
					bIgnoreImmunity = true}*/

		destroyable->SetIsImmune(false);
		destroyable->SetFaction(4);

		Game::entityManager->SerializeEntity(self);

	} else if (timerName == "Clear") {
		Game::entityManager->FireEventServerSide(self, "ClearProperty");
		self->CancelAllTimers();
	} else if (timerName == "UnlockSpecials") {
		//We no longer need to lock specials
		self->SetBoolean(u"bSpecialLock", false);

		//Did we queue a spcial attack?
		if (self->GetBoolean(u"bSpecialQueued")) {
			self->SetBoolean(u"bSpecialQueued", false);
		}
	}
}

void BossSpiderQueenEnemyServer::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	if (m_CurrentBossStage > 0 && !self->HasTimer("RFS")) {
		self->AddTimer("RFS", 5.0f);
	}

	if (m_CurrentBossStage > 0 && !self->HasTimer("ROF")) {
		self->AddTimer("ROF", 10.0f);
	}

	if (m_CurrentBossStage > ThresholdTable.size()) {
		return;
	}

	int currentThreshold = ThresholdTable[m_CurrentBossStage - 1];

	if (destroyable->GetHealth() <= currentThreshold) {
		auto isWithdrawn = self->GetBoolean(u"isWithdrawn");

		if (!isWithdrawn) {
			self->CancelAllTimers();

			self->SetBoolean(u"isSpecialAttacking", false);
			self->SetBoolean(u"bSpecialLock", false);

			WithdrawSpider(self, true);
		}
	}
}

void BossSpiderQueenEnemyServer::OnUpdate(Entity* self) {
	auto isWithdrawn = self->GetBoolean(u"isWithdrawn");

	if (!isWithdrawn) return;

	if (controllable->GetRotation() == NiQuaternion::IDENTITY) {
		return;
	}

	controllable->SetStatic(false);
	controllable->SetRotation(NiQuaternion::IDENTITY);
	controllable->SetStatic(true);

	Game::entityManager->SerializeEntity(self);
}

//----------------------------------------------
//--Utility function capable of playing a priority
//-- animation on a targetand returning either the
//-- anim time, or a desired default
//----------------------------------------------
float BossSpiderQueenEnemyServer::PlayAnimAndReturnTime(Entity* self, const std::u16string& animID) {
	//TODO: Get the actual animation time

	// Get the anim time
	float animTimer = RenderComponent::GetAnimationTime(self, animID);

	// If we have an animation play it
	if (animTimer > 0) {
		animTimer = RenderComponent::PlayAnimation(self, animID);
	}

	// If the anim time is less than the the default time use default
	if (animTimer < defaultAnimPause) {
		animTimer = defaultAnimPause;
	}

	return animTimer;
}
