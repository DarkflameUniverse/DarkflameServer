#include "SGCannon.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "dZoneManager.h"
#include "Character.h"
#include "ShootingGalleryComponent.h"
#include "PossessorComponent.h"
#include "CharacterComponent.h"
#include "SimplePhysicsComponent.h"
#include "MovementAIComponent.h"
#include "ObjectIDManager.h"
#include "MissionComponent.h"
#include "Loot.h"
#include "InventoryComponent.h"
#include "eMissionTaskType.h"
#include "eReplicaComponentType.h"
#include "RenderComponent.h"
#include "eGameActivity.h"
#include "Item.h"
#include <ranges>

void SGCannon::OnStartup(Entity* self) {
	LOG("OnStartup");

	m_Waves = GetWaves();
	constants = GetConstants();

	ResetVars(self);

	self->SetVar<bool>(GameStartedVariable, false);
	self->SetVar<Vector3>(InitialVelocityVariable, {});
	self->SetVar<uint32_t>(ImpactSkillVariale, constants.impactSkillID);

	auto* shootingGalleryComponent = self->GetComponent<ShootingGalleryComponent>();
	if (shootingGalleryComponent != nullptr) {
		shootingGalleryComponent->SetStaticParams({
			Vector3 { -327.8609924316406, 256.8999938964844, 1.6482199430465698 },
			Vector3 { -181.4320068359375, 212.39999389648438, 2.5182199478149414 }
			});

		shootingGalleryComponent->SetDynamicParams({
			Vector3 { 0.0, 4.3, 9.0 },
			Vector3 { },
			129.0,
			800.0,
			30.0,
			0.0,
			-1.0,
			58.6
			});
	}

	self->SetVar<uint32_t>(TimeLimitVariable, 30);
	self->SetVar<std::vector<LOT>>(ValidActorsVariable, { 3109, 3110, 3111, 3112, 3125, 3126 });
	self->SetVar<std::vector<LOT>>(ValidEffectsVariable, { 3122 });
	self->SetVar<std::vector<uint32_t>>(StreakBonusVariable, { 1, 2, 5, 10 });
	self->SetVar<bool>(SuperChargeActiveVariable, false);
	self->SetVar<uint32_t>(MatrixVariable, 1);
	self->SetVar<bool>(InitVariable, true);

	auto* simplePhysicsComponent = self->GetComponent<SimplePhysicsComponent>();

	if (simplePhysicsComponent != nullptr) {
		simplePhysicsComponent->SetPhysicsMotionState(5);
	}
	Game::entityManager->SerializeEntity(self);
}

void SGCannon::OnPlayerLoaded(Entity* self, Entity* player) {
	LOG("Player loaded");
	self->SetVar<LWOOBJID>(PlayerIDVariable, player->GetObjectID());
}

void SGCannon::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	Script::OnFireEventServerSide(self, sender, args, param1, param2, param3);
}

void SGCannon::OnActivityStateChangeRequest(Entity* self, LWOOBJID senderID, int32_t value1, int32_t value2,
	const std::u16string& stringValue) {
	LOG("Got activity state change request: %s", GeneralUtils::UTF16ToWTF8(stringValue).c_str());
	if (stringValue == u"clientready") {
		auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
		if (player != nullptr) {
			LOG("Player is ready");

			GameMessages::SendActivityEnter(self->GetObjectID(), player->GetSystemAddress());

			auto* shootingGalleryComponent = self->GetComponent<ShootingGalleryComponent>();

			if (shootingGalleryComponent != nullptr) {
				shootingGalleryComponent->SetCurrentPlayerID(player->GetObjectID());

				LOG("Setting player ID");

				Game::entityManager->SerializeEntity(self);
			} else {
				LOG("Shooting gallery component is null");
			}

			auto* characterComponent = player->GetComponent<CharacterComponent>();

			if (characterComponent != nullptr) {
				characterComponent->SetCurrentActivity(eGameActivity::SHOOTING_GALLERY);
				auto possessor = player->GetComponent<PossessorComponent>();
				if (possessor) {
					possessor->SetPossessable(self->GetObjectID());
					possessor->SetPossessableType(ePossessionType::NO_POSSESSION);
				}

				Game::entityManager->SerializeEntity(player);
			}

			self->AddCallbackTimer(1.0f, [self, this]() {
				self->SetNetworkVar<bool>(HideScoreBoardVariable, true);
				self->SetNetworkVar<bool>(ReSetSuperChargeVariable, true);
				self->SetNetworkVar<bool>(ShowLoadingUI, true);
				});

		} else {
			LOG("Player not found");
		}
	} else if (value1 == 1200) {
		StartGame(self);
	}
}

void SGCannon::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
	if (!player) return;

	if (identifier == u"Scoreboardinfo") {
		GameMessages::SendDisplayMessageBox(player->GetObjectID(), true,
			Game::zoneManager->GetZoneControlObject()->GetObjectID(),
			u"Shooting_Gallery_Retry", 2, u"Retry?",
			u"", player->GetSystemAddress());
	} else {
		if ((button == 1 && (identifier == u"Shooting_Gallery_Retry" || identifier == u"RePlay")) || identifier == u"SG1" || button == 0) {
			if (IsPlayerInActivity(self, player->GetObjectID())) return;
			self->SetNetworkVar<bool>(ClearVariable, true);
			StartGame(self);
		} else if (button == 0 && ((identifier == u"Shooting_Gallery_Retry" || identifier == u"RePlay"))) {
			RemovePlayer(player->GetObjectID());
			UpdatePlayer(self, player->GetObjectID(), true);
		} else if (button == 1 && identifier == u"Shooting_Gallery_Exit") {
			UpdatePlayer(self, player->GetObjectID(), true);
			RemovePlayer(player->GetObjectID());
		}
	}
}

void SGCannon::SuperChargeTimerFunc(Entity* self) {
	if (self->GetVar<bool>(WaveStatusVariable) || self->GetVar<uint32_t>(CurrentSuperChargedTimeVariable) < 1) {
		self->SetNetworkVar<uint32_t>(ChargeCountingVariable, 99);
		self->SetNetworkVar<uint32_t>(SuperChargeBarVariable, 0);
		ToggleSuperCharge(self, false);
	}
}

void SGCannon::SpawnWaveTimerFunc(Entity* self) {
	if (self->GetVar<bool>(GameStartedVariable)) {
		self->SetVar<bool>(WaveStatusVariable, true);
		const auto wave = static_cast<int32_t>(self->GetVar<uint32_t>(ThisWaveVariable));

		if (wave != 0 && self->GetVar<bool>(SuperChargePausedVariable)) {
			StartChargedCannon(self, self->GetVar<uint32_t>(CurrentSuperChargedTimeVariable));
			self->SetVar<uint32_t>(CurrentSuperChargedTimeVariable, 0);
		}

		TimerToggle(self, true);

		for (const auto& enemyToSpawn : m_Waves.at(self->GetVar<uint32_t>(ThisWaveVariable))) {
			SpawnObject(self, enemyToSpawn, true);
		}

		LOG("Current wave spawn: %i/%i", wave, m_Waves.size());

		// All waves completed
		const auto timeLimit = static_cast<float_t>(self->GetVar<uint32_t>(TimeLimitVariable));
		if (wave >= m_Waves.size()) {
			ActivityTimerStart(self, GameOverTimer, timeLimit, timeLimit);
		} else {
			ActivityTimerStart(self, EndWaveTimer, timeLimit, timeLimit);
		}

		const auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
		if (player != nullptr) {
			GameMessages::SendPlayFXEffect(player->GetObjectID(), -1, u"SG-start", "");

			GameMessages::SendStartActivityTime(self->GetObjectID(), timeLimit, player->GetSystemAddress());
			LOG("Sending ActivityPause false");

			GameMessages::SendActivityPause(self->GetObjectID(), false, player->GetSystemAddress());
		}
	}
}

void SGCannon::EndWaveTimerFunc(Entity* self) {
	self->SetVar<bool>(WaveStatusVariable, false);
	TimerToggle(self);
	RecordPlayerScore(self);

	if (self->GetVar<uint32_t>(ThisWaveVariable) >= 2) {
		GameMessages::SendActivityPause(self->GetObjectID(), true);
		ActivityTimerStart(self, GameOverTimer, 0.1, 0.1);
		return;
	}

	self->SetVar<uint32_t>(ThisWaveVariable, self->GetVar<uint32_t>(ThisWaveVariable) + 1);
	PlaySceneAnimation(self, u"wave" + GeneralUtils::to_u16string(self->GetVar<uint32_t>(ThisWaveVariable)), true, true, 1.7f);
	self->SetNetworkVar<uint32_t>(WaveNumVariable, self->GetVar<uint32_t>(ThisWaveVariable) + 1);
	self->SetNetworkVar<uint32_t>(WaveStrVariable, self->GetVar<uint32_t>(TimeLimitVariable));

	LOG("Current wave: %i/%i", self->GetVar<uint32_t>(ThisWaveVariable), m_Waves.size());

	if (self->GetVar<uint32_t>(ThisWaveVariable) >= m_Waves.size()) {
		ActivityTimerStart(self, GameOverTimer, 0.1, 0.1);
	} else {
		ActivityTimerStart(self, SpawnWaveTimer, constants.inBetweenWavePause, constants.inBetweenWavePause);
	}

	LOG("Sending ActivityPause true");

	GameMessages::SendActivityPause(self->GetObjectID(), true);
	if (self->GetVar<bool>(SuperChargeActiveVariable) && !self->GetVar<bool>(SuperChargePausedVariable)) {
		PauseChargeCannon(self);
	}
}

void SGCannon::GameOverTimerFunc(Entity* self) {
	auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
	if (player != nullptr) {
		LOG_DEBUG("Sending ActivityPause true");

		GameMessages::SendActivityPause(self->GetObjectID(), true, player->GetSystemAddress());

		ActivityTimerStart(self, EndGameBufferTimer, 1, 1);

		TimerToggle(self);
	}
}

void SGCannon::DoSpawnTimerFunc(Entity* self, const std::string& name) {
	if (self->GetVar<bool>(GameStartedVariable)) {
		const auto spawnNumber = static_cast<uint32_t>(std::stoi(name.substr(7)));
		const auto& activeSpawns = self->GetVar<std::vector<SGEnemy>>(ActiveSpawnsVariable);
		if (activeSpawns.size() <= spawnNumber) {
			LOG_DEBUG("Trying to spawn %i when spawns size is only %i", spawnNumber, activeSpawns.size());
			return;
		}

		const auto& toSpawn = activeSpawns.at(spawnNumber);
		const auto pathIndex = GeneralUtils::GenerateRandomNumber<size_t>(0, toSpawn.spawnPaths.size() - 1);
		const auto* path = Game::zoneManager->GetZone()->GetPath(toSpawn.spawnPaths.at(pathIndex));
		if (!path || path->pathWaypoints.empty()) {
			LOG_DEBUG("Path %s at index %i or has 0 waypoints", toSpawn.spawnPaths.at(pathIndex).c_str(), pathIndex);
			return;
		}

		auto info = EntityInfo{};
		info.lot = toSpawn.lot;
		info.spawnerID = self->GetObjectID();
		info.pos = path->pathWaypoints[0].position;

		info.settings = {
			new LDFData<SGEnemy>(u"SpawnData", toSpawn),
			new LDFData<std::string>(u"custom_script_server", "scripts/ai/ACT/SG_TARGET.lua"), // this script is never loaded
			new LDFData<std::string>(u"custom_script_client", "scripts/client/ai/SG_TARGET_CLIENT.lua"),
			new LDFData<std::string>(u"attached_path", path->pathName),
			new LDFData<uint32_t>(u"attached_path_start", 0),
			new LDFData<std::u16string>(u"groupID", u"SGEnemy"),
			new LDFData<uint32_t>(u"wave", self->GetVar<uint32_t>(ThisWaveVariable)),
		};

		auto* enemy = Game::entityManager->CreateEntity(info, nullptr, self);

		auto* movementAI = enemy->AddComponent<MovementAIComponent>(MovementAIInfo{});
		auto* simplePhysicsComponent = enemy->GetComponent<SimplePhysicsComponent>();
		if (simplePhysicsComponent) {
			simplePhysicsComponent->SetPhysicsMotionState(4);
		}

		Game::entityManager->ConstructEntity(enemy);

		movementAI->SetMaxSpeed(toSpawn.initialSpeed);
		movementAI->SetCurrentSpeed(toSpawn.initialSpeed);
		movementAI->SetHaltDistance(0.0f);

		movementAI->SetPath(path->pathWaypoints);

		enemy->AddDieCallback([this, self, enemy, name]() {
			RegisterHit(self, enemy, name);
			});

		// Save the enemy and tell it to start pathing
		if (enemy != nullptr) {
			const_cast<std::vector<LWOOBJID>&>(self->GetVar<std::vector<LWOOBJID>>(SpawnedObjects)).push_back(enemy->GetObjectID());
			GameMessages::SendPlatformResync(enemy, UNASSIGNED_SYSTEM_ADDRESS);
		}
	}
}

void SGCannon::EndGameBufferTimerFunc(Entity* self) {
	RecordPlayerScore(self);
	StopGame(self, false);
}

void SGCannon::OnActivityTimerDone(Entity* self, const std::string& name) {
	if (name == SuperChargeTimer && !self->GetVar<bool>(SuperChargePausedVariable)) {
		SuperChargeTimerFunc(self);
	} else if (name == SpawnWaveTimer) {
		SpawnWaveTimerFunc(self);
	} else if (name == EndWaveTimer) {
		EndWaveTimerFunc(self);
	} else if (name == GameOverTimer) {
		GameOverTimerFunc(self);
	} else if (name.rfind(DoSpawnTimer, 0) == 0) {
		DoSpawnTimerFunc(self, name);
	} else if (name == EndGameBufferTimer) {
		EndGameBufferTimerFunc(self);
	}
}

void
SGCannon::OnActivityTimerUpdate(Entity* self, const std::string& name, float_t timeRemaining, float_t elapsedTime) {
	ActivityManager::OnActivityTimerUpdate(self, name, timeRemaining, elapsedTime);
}

void SGCannon::StartGame(Entity* self) {
	if (self->GetVar<bool>(GameStartedVariable)) return;
	self->SetNetworkVar<uint32_t>(TimeLimitVariable, self->GetVar<uint32_t>(TimeLimitVariable));
	self->SetNetworkVar<bool>(AudioStartIntroVariable, true);
	self->SetVar<LOT>(CurrentRewardVariable, LOT_NULL);

	auto rewardObjects = Game::entityManager->GetEntitiesInGroup(constants.rewardModelGroup);
	for (auto* reward : rewardObjects) {
		GameMessages::SetModelToBuild modelToBuild{};
		modelToBuild.modelLot = LOT_NULL;
		modelToBuild.target = reward->GetObjectID();
		modelToBuild.Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
	if (player != nullptr) {
		// The client cant accept more than 10 results.
		GetLeaderboardData(self, player->GetObjectID(), GetConstants().activityID, 10);
		LOG("Sending ActivityStart");
		GameMessages::SendActivityStart(self->GetObjectID(), player->GetSystemAddress());

		GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"start", "");

		self->SetNetworkVar<bool>(ClearVariable, true);
		DoGameStartup(self);

		if (!self->GetVar<bool>(FirstTimeDoneVariable)) {
			TakeActivityCost(self, player->GetObjectID());
		}

		self->SetVar<bool>(FirstTimeDoneVariable, true);
	}

	SpawnNewModel(self);
}

void SGCannon::DoGameStartup(Entity* self) {
	ResetVars(self);
	self->SetVar<bool>(GameStartedVariable, true);
	self->SetNetworkVar<bool>(ClearVariable, true);
	self->SetVar<uint32_t>(ThisWaveVariable, 0);

	if (constants.firstWaveStartTime < 1) {
		constants.firstWaveStartTime = 1;
	}

	ActivityTimerStart(self, SpawnWaveTimer, constants.firstWaveStartTime,
		constants.firstWaveStartTime);
}

void SGCannon::OnShootingGalleryFire(Entity& self, GameMessages::ShootingGalleryFire& fire) {
	self.SetVar<uint32_t>(ShotsFiredVariable, self.GetVar<uint32_t>(ShotsFiredVariable) + 1);
}

void SGCannon::SpawnNewModel(Entity* self) {

	// Add a new reward to the existing rewards
	const auto currentReward = self->GetVar<LOT>(CurrentRewardVariable);
	if (currentReward != -1) {
		auto rewards = self->GetVar<std::vector<LOT>>(RewardsVariable);
		rewards.push_back(currentReward);
		self->SetVar<std::vector<LOT>>(RewardsVariable, rewards);
		self->SetNetworkVar<int32_t>(RewardAddedVariable, currentReward);
	}

	auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
	if (player != nullptr) {
		for (auto* rewardModel : Game::entityManager->GetEntitiesInGroup(constants.rewardModelGroup)) {
			uint32_t lootMatrix;
			switch (self->GetVar<uint32_t>(MatrixVariable)) {
			case 1:
				lootMatrix = constants.scoreLootMatrix1;
				break;
			case 2:
				lootMatrix = constants.scoreLootMatrix2;
				break;
			case 3:
				lootMatrix = constants.scoreLootMatrix3;
				break;
			case 4:
				lootMatrix = constants.scoreLootMatrix4;
				break;
			case 5:
				lootMatrix = constants.scoreLootMatrix5;
				break;
			default:
				lootMatrix = 0;
			}

			if (lootMatrix != 0) {
				std::unordered_map<LOT, int32_t> toDrop = {};
				toDrop = Loot::RollLootMatrix(player, lootMatrix);

				for (const auto [lot, count] : toDrop) {
					GameMessages::SetModelToBuild modelToBuild{};
					modelToBuild.modelLot = lot;
					modelToBuild.target = rewardModel->GetObjectID();
					modelToBuild.Send(player->GetSystemAddress());

					self->SetVar<LOT>(CurrentRewardVariable, lot);
				}
			}
		}
	}
}

void SGCannon::RemovePlayer(LWOOBJID playerID) {
	auto* player = Game::entityManager->GetEntity(playerID);
	if (!player) return;

	auto* character = player->GetCharacter();
	auto* characterComponent = player->GetComponent<CharacterComponent>();
	if (characterComponent && character) {
		characterComponent->SendToZone(character->GetLastNonInstanceZoneID());
	}
}

void SGCannon::OnRequestActivityExit(Entity* self, LWOOBJID player, bool canceled) {
	if (canceled) {
		StopGame(self, canceled);
		RemovePlayer(player);
	}
}


void SGCannon::StartChargedCannon(Entity* self, uint32_t optionalTime) {
	optionalTime = optionalTime == 0 ? constants.chargedTime : optionalTime;
	self->SetVar<bool>(SuperChargePausedVariable, false);
	ToggleSuperCharge(self, true);
	ActivityTimerStart(self, SuperChargeTimer, 1, optionalTime);

	if (!self->GetVar<bool>(WaveStatusVariable)) {
		PauseChargeCannon(self);
	}
}

void SGCannon::TimerToggle(Entity* self, bool start) {
	if (start) {
		self->SetNetworkVar<uint32_t>(CountVariable, self->GetVar<uint32_t>(TimeLimitVariable));
		self->SetVar<bool>(GameStartedVariable, true);
	} else {
		self->SetNetworkVar<bool>(StopVariable, true);
	}
}

void SGCannon::SpawnObject(Entity* self, const SGEnemy& toSpawn, bool spawnNow) {
	auto activeSpawns = self->GetVar<std::vector<SGEnemy>>(ActiveSpawnsVariable);
	activeSpawns.push_back(toSpawn);
	self->SetVar<std::vector<SGEnemy>>(ActiveSpawnsVariable, activeSpawns);

	self->SetVar(SpawnNumberVariable, activeSpawns.size() - 1);
	const auto timerName = DoSpawnTimer + std::to_string(activeSpawns.size() - 1);

	if (spawnNow) {
		if (toSpawn.minSpawnTime > 0 && toSpawn.maxSpawnTime > 0) {
			const auto spawnTime = GeneralUtils::GenerateRandomNumber<float_t>(toSpawn.minSpawnTime, toSpawn.maxSpawnTime);

			ActivityTimerStart(self, timerName, spawnTime, spawnTime);
		} else {
			ActivityTimerStart(self, timerName, 1, 1);
		}
	} else if (toSpawn.respawns) {
		const auto spawnTime = GeneralUtils::GenerateRandomNumber<float_t>(toSpawn.minRespawnTime, toSpawn.maxRespawnTime);

		ActivityTimerStart(self, timerName, spawnTime, spawnTime);
	}
}

void SGCannon::RecordPlayerScore(Entity* self) {
	const auto totalScore = self->GetVar<int32_t>(TotalScoreVariable);
	const auto currentWave = self->GetVar<uint32_t>(ThisWaveVariable);

	if (currentWave > 0) {
		auto totalWaveScore = totalScore;
		auto playerScores = self->GetVar<std::vector<int32_t>>(PlayerScoresVariable);

		for (const auto& waveScore : playerScores) {
			totalWaveScore -= waveScore;
		}

		if (currentWave >= playerScores.size()) {
			playerScores.push_back(totalWaveScore);
		} else {
			playerScores[currentWave] = totalWaveScore;
		}
		self->SetVar<std::vector<int32_t>>(PlayerScoresVariable, playerScores);
	}
}

void SGCannon::PlaySceneAnimation(Entity* self, const std::u16string& animationName, bool onCannon, bool onPlayer, float_t priority) {
	for (auto* cannon : Game::entityManager->GetEntitiesInGroup("cannongroup")) {
		RenderComponent::PlayAnimation(cannon, animationName, priority);
	}

	if (onCannon) {
		RenderComponent::PlayAnimation(self, animationName, priority);
	}

	if (onPlayer) {
		auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
		if (player != nullptr) {
			RenderComponent::PlayAnimation(player, animationName, priority);
		}
	}
}

void SGCannon::PauseChargeCannon(Entity* self) {
	const auto time = std::max(static_cast<uint32_t>(std::ceil(ActivityTimerGetRemainingTime(self, SuperChargeTimer))), static_cast<uint32_t>(1));

	self->SetVar<bool>(SuperChargePausedVariable, true);
	self->SetVar<uint32_t>(CurrentSuperChargedTimeVariable, time);
	self->SetNetworkVar<uint32_t>(ChargeCountingVariable, time);
	ActivityTimerStop(self, SuperChargeTimer);
}

void SGCannon::StopGame(Entity* self, bool cancel) {
	self->SetNetworkVar<bool>(ReSetSuperChargeVariable, true);
	self->SetNetworkVar<bool>(HideSuperChargeVariable, true);

	auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
	if (player == nullptr)
		return;

	ToggleSuperCharge(self, false);

	// The player won, store all the score and send rewards
	if (!cancel) {
		float percentage = 0.0f;
		float misses = self->GetVar<uint32_t>(MissesVariable);
		float fired = self->GetVar<uint32_t>(ShotsFiredVariable);

		if (fired > 0.0f) {
			percentage = misses / fired;
		}

		percentage = 1.0f - percentage;
		percentage = std::max(percentage, 0.0f);

		auto* missionComponent = player->GetComponent<MissionComponent>();

		if (missionComponent != nullptr) {
			missionComponent->Progress(eMissionTaskType::PERFORM_ACTIVITY, self->GetVar<int32_t>(TotalScoreVariable), self->GetObjectID(), "performact_score");
			missionComponent->Progress(eMissionTaskType::PERFORM_ACTIVITY, self->GetVar<uint32_t>(MaxStreakVariable), self->GetObjectID(), "performact_streak");
			missionComponent->Progress(eMissionTaskType::ACTIVITY, m_CannonLot, 0, "", self->GetVar<int32_t>(TotalScoreVariable));
		}

		Loot::GiveActivityLoot(player, self->GetObjectID(), GetGameID(self), self->GetVar<int32_t>(TotalScoreVariable));

		SaveScore(self, player->GetObjectID(),
			static_cast<float>(self->GetVar<int32_t>(TotalScoreVariable)), static_cast<float>(self->GetVar<uint32_t>(MaxStreakVariable)), percentage);

		StopActivity(self, player->GetObjectID(), self->GetVar<int32_t>(TotalScoreVariable), self->GetVar<uint32_t>(MaxStreakVariable), percentage);
		self->SetNetworkVar<bool>(AudioFinalWaveDoneVariable, true);

		// Give the player the model rewards they earned
		auto* inventory = player->GetComponent<InventoryComponent>();
		if (inventory != nullptr) {
			for (const auto rewardLot : self->GetVar<std::vector<LOT>>(RewardsVariable)) {
				inventory->AddItem(rewardLot, 1, eLootSourceType::NONE, eInventoryType::MODELS);
			}
		}

		const auto& waveScores = self->GetVar<std::vector<int32_t>>(PlayerScoresVariable);
		std::stringstream stream;

		stream << self->GetVar<int32_t>(TotalScoreVariable) << "_";

		// technically unused in shooting gallery but serialize it regardless.
		for (const auto& score : waveScores) {
			stream << score << "_";
		}
		auto totalmissed = fired - misses;
		if (totalmissed < 0) {
			totalmissed = 0;
		}

		stream << fired << "_" << totalmissed << "_" << self->GetVar<uint32_t>(MaxStreakVariable);

		self->SetNetworkVar<std::u16string>(u"UI_Rewards", GeneralUtils::ASCIIToUTF16(stream.str()));
	}

	GameMessages::SendActivityStop(self->GetObjectID(), false, cancel, player->GetSystemAddress());
	self->SetVar<bool>(GameStartedVariable, false);
	ActivityTimerStopAllTimers(self);

	// Destroy all spawners
	for (auto* entity : Game::entityManager->GetEntitiesInGroup("SGEnemy")) {
		entity->Smash(LWOOBJID_EMPTY, eKillType::SILENT);
	}

	ResetVars(self);
}


void SGCannon::OnActivityNotify(Entity* self, GameMessages::ActivityNotify& notify) {
	if (!self->GetVar<bool>(GameStartedVariable)) return;

	const auto& params = notify.notification;
	if (params.empty()) return;

	const auto& param = params[0];
	if (param->GetValueType() != LDF_TYPE_S32 || param->GetKey() != u"shot_done") return;

	const auto superChargeShotDone = static_cast<LDFData<int32_t>*>(param.get())->GetValue() == GetConstants().cannonSuperChargeSkill;

	const auto& hitTargets = self->GetVar<std::vector<LWOOBJID>>(u"CannonBallKills");

	if (hitTargets.empty() && !superChargeShotDone) {
		self->SetVar<uint32_t>(u"m_curStreak", 0);
		self->SetVar<uint32_t>(MissesVariable, self->GetVar<uint32_t>(MissesVariable) + 1);
		self->SetNetworkVar<bool>(u"HideStreak", true);
		self->SetNetworkVar<bool>(u"UnMarkAll", true);
		UpdateStreak(self);
	} else if (hitTargets.size() > 1) {
		self->SetNetworkVar<bool>(u"mHit", true);
	}

	self->SetVar<std::vector<LWOOBJID>>(u"CannonBallKills", {});
}

void SGCannon::RegisterHit(Entity* self, Entity* target, const std::string& timerName) {
	if (!self->GetVar<bool>(GameStartedVariable)) return;

	auto cannonBallKills = self->GetVar<std::vector<LWOOBJID>>(u"CannonBallKills");
	cannonBallKills.push_back(target->GetObjectID());
	self->SetVar<std::vector<LWOOBJID>>(u"CannonBallKills", cannonBallKills);
	const auto& spawnInfo = target->GetVar<SGEnemy>(u"SpawnData");

	if (spawnInfo.respawns && target->GetVar<uint32_t>(u"wave") == self->GetVar<uint32_t>(ThisWaveVariable)) {
		const auto respawnTime = GeneralUtils::GenerateRandomNumber<float_t>(spawnInfo.minRespawnTime, spawnInfo.maxRespawnTime);

		ActivityTimerStart(self, timerName, respawnTime, respawnTime);
	}

	int32_t score = spawnInfo.score;

	if (score > 0) {
		score += score * GetCurrentBonus(self);

		if (!self->GetVar<bool>(SuperChargeActiveVariable)) {
			self->SetVar<uint32_t>(u"m_curStreak", self->GetVar<uint32_t>(u"m_curStreak") + 1);
		}
	} else {
		if (!self->GetVar<bool>(SuperChargeActiveVariable)) {
			self->SetVar<uint32_t>(u"m_curStreak", 0);
			self->SetVar<uint32_t>(MissesVariable, self->GetVar<uint32_t>(MissesVariable) + 1);
		}

		self->SetNetworkVar<bool>(u"hitFriend", true);
	}

	auto lastSuperTotal = self->GetVar<uint32_t>(u"LastSuperTotal");

	auto scScore = self->GetVar<int32_t>(TotalScoreVariable) - lastSuperTotal;

	if (!self->GetVar<bool>(SuperChargeActiveVariable) && scScore >= constants.chargedPoints && score >= 0) {
		StartChargedCannon(self);
		self->SetNetworkVar<float>(u"SuperChargeBar", 100.0f);
		self->SetVar<uint32_t>(u"LastSuperTotal", self->GetVar<int32_t>(TotalScoreVariable));
	}

	UpdateStreak(self);

	GameMessages::SendNotifyClientShootingGalleryScore(self->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS,
		0.0f,
		score,
		target->GetObjectID(),
		target->GetPosition()
	);

	auto newScore = self->GetVar<int32_t>(TotalScoreVariable) + score;

	if (newScore < 0) {
		newScore = 0;
	}

	self->SetVar<int32_t>(TotalScoreVariable, newScore);

	self->SetNetworkVar<uint32_t>(u"updateScore", newScore);

	self->SetNetworkVar<std::u16string>(u"beatHighScore", GeneralUtils::to_u16string(newScore));

	auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
	if (player == nullptr) return;

	auto missionComponent = player->GetComponent<MissionComponent>();
	if (missionComponent == nullptr) return;

	missionComponent->Progress(eMissionTaskType::SMASH, spawnInfo.lot, self->GetObjectID());

	auto matrix = self->GetVar<uint32_t>(MatrixVariable);

	float rewardS = 0.0f;
	float rewardF = 0.0f;
	if (matrix <= 5) {
		const auto scoreRewardNum = "Score_Reward_" + std::to_string(matrix);
		const auto rewardAmountItr = constants.scoreRewards.find(scoreRewardNum);
		if (rewardAmountItr != constants.scoreRewards.end()) {
			const float rewardAmount = rewardAmountItr->second / 100 * 3;
			rewardS = newScore / rewardAmount;
			rewardF = std::round(rewardS * 3);

			if (rewardF > 100.0f) rewardF = 100.0f;

			self->SetNetworkVar(ModelPercentVariable, rewardF);
		}
	}

	if (rewardF > 0.0f && rewardF < 200.0f && matrix <= 5) {
		const auto rewardModelGroup = Game::entityManager->GetEntitiesInGroup(constants.rewardModelGroup);
		if (!rewardModelGroup.empty()) {
			auto* rewardModel = rewardModelGroup[0];
			GameMessages::SpawnModelBricks spawnBricks{};
			spawnBricks.target = rewardModel->GetObjectID();
			spawnBricks.amount = rewardF / 100.0f;
			spawnBricks.position = target->GetPosition();
			spawnBricks.Send(player->GetSystemAddress());
			if (rewardF >= 100.0f) {
				SpawnNewModel(self);
				self->SetVar<uint32_t>(MatrixVariable, matrix + 1);
			}
		}
	}
}

void SGCannon::UpdateStreak(Entity* self) {
	const auto streakBonus = GetCurrentBonus(self);

	const auto curStreak = self->GetVar<uint32_t>(u"m_curStreak");

	const auto marks = curStreak % 3;

	self->SetNetworkVar<uint32_t>(u"cStreak", curStreak);

	if (curStreak >= 0 && curStreak < 13) {
		if (marks == 1) {
			self->SetNetworkVar<bool>(u"Mark1", true);
		} else if (marks == 2) {
			self->SetNetworkVar<bool>(u"Mark2", true);
		} else if (marks == 0 && curStreak > 0) {
			self->SetVar<float_t>(u"StreakBonus", streakBonus);
			self->SetNetworkVar<bool>(u"ShowStreak", streakBonus + 1);
			self->SetNetworkVar<bool>(u"Mark3", true);
		} else {
			self->SetVar<float_t>(u"StreakBonus", streakBonus);
			self->SetNetworkVar<bool>(u"UnMarkAll", true);
		}
	}
	auto maxStreak = self->GetVar<uint32_t>(MaxStreakVariable);
	if (maxStreak < curStreak) self->SetVar<uint32_t>(MaxStreakVariable, curStreak);
}

float_t SGCannon::GetCurrentBonus(Entity* self) {
	auto streak = self->GetVar<uint32_t>(u"m_curStreak");

	if (streak > 12) {
		streak = 12;
	}

	return streak / 3;
}

void SGCannon::ToggleSuperCharge(Entity* self, bool enable) {
	if (enable && self->GetVar<bool>(SuperChargeActiveVariable))
		return;

	auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));

	if (player == nullptr) {
		LOG("Player not found in toggle super charge");
		return;
	}

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	auto equippedItems = inventoryComponent->GetEquippedItems();

	LOG("Player has %d equipped items", equippedItems.size());

	auto skillID = constants.cannonSkill;
	auto cooldown = constants.cannonRefireRate;

	auto* selfInventoryComponent = self->GetComponent<InventoryComponent>();

	// This is a gm in the original script
	Item* meItem1{};
	Item* meItem2{};
	for (const auto item : selfInventoryComponent->GetInventory(eInventoryType::ITEMS)->GetItems() | std::views::values) {
		if (item->GetSlot() == 0) meItem1 = item;
		else if (item->GetSlot() == 1) meItem2 = item;
	}

	if (!meItem1 || !meItem2) {
		LOG("Cannon does not have the required items equipped");
		return;
	}

	if (enable) {
		LOG("Player is activating super charge");
		selfInventoryComponent->EquipItem(meItem1);
		selfInventoryComponent->EquipItem(meItem2);

		skillID = constants.cannonSuperChargeSkill;
		cooldown = 400;
	} else {
		selfInventoryComponent->UnEquipItem(meItem1);
		selfInventoryComponent->UnEquipItem(meItem2);

		self->SetNetworkVar<float>(u"SuperChargeBar", 0);

		LOG("Player disables super charge");

		cooldown = 800;
		self->SetVar<uint32_t>(NumberOfChargesVariable, 0);
	}

	const auto& constants = GetConstants();

	auto* shootingGalleryComponent = self->GetComponent<ShootingGalleryComponent>();

	if (shootingGalleryComponent == nullptr) {
		return;
	}

	DynamicShootingGalleryParams properties = shootingGalleryComponent->GetDynamicParams();

	properties.cannonFOV = constants.cannonFOV;
	properties.cannonVelocity = constants.cannonVelocity;
	properties.cannonRefireRate = cooldown;
	properties.cannonMinDistance = constants.cannonMinDistance;
	properties.cannonTimeout = -1;

	shootingGalleryComponent->SetDynamicParams(properties);

	Game::entityManager->SerializeEntity(self);
	Game::entityManager->SerializeEntity(player);

	self->SetNetworkVar<uint64_t>(CannonBallSkillIDVariable, skillID);
	self->SetVar<bool>(SuperChargeActiveVariable, enable);
}

std::vector<std::vector<SGEnemy>> SGCannon::GetWaves() {
	return {
		// Wave 1
		{
			// Ship 1
			{
				std::vector<std::string> { "Wave_1_Ship_1", "Wave_1_Ship_3" },
				6015, 0.0, 2.0, true, 0.0, 2.0,
				2.0, 1500, false, 0.0, 1.0,
				1.0, false, true
			},

		// Ship 2
		{
			std::vector<std::string> { "Wave_1_Ship_2", "Wave_1_Ship_4" },
			6300, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 500, false, 0.0, 1.0,
			1.0, false, true
		},

		// Sub 1 but for dlu
		{
			std::vector<std::string> { "Wave_1_Sub_1", "Wave_1_Sub_2" },
			6016, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 1000, false, 0.0, 1.0,
			1.0, true, true
		},

		//// Sub 1
		//{
		//	std::vector<std::string> { "Wave_1_Sub_1", "Wave_1_Sub_2" },
		//	6016, 0.0, 2.0, true, 0.0, 2.0,
		//	10.0, 1000, false, 0.0, 1.0,
		//	1.0, true, true
		//},

		// Sub 2 but for dlu
		{
			std::vector<std::string> { "Wave_2_Sub_1", "Wave_2_Sub_2" },
			6016, 0.0, 2.0, true, 0.0, 2.0,
			3.0, 1000, false, 0.0, 1.0,
			1.0, true, true
		},

		// Sub 2
		//{
		//	std::vector<std::string> { "Wave_2_Sub_1", "Wave_2_Sub_2" },
		//	6016, 0.0, 2.0, true, 0.0, 2.0,
		//	2.0, 1000, false, 0.0, 1.0,
		//	1.0, true, true
		//},

		// Friendly
		{
			std::vector<std::string> { "Wave_3_FShip_1", "Wave_3_FShip_2" },
			2168,0.0,5.0,true, 2.0, 5.0,
			1.0, -1000, false, 0.0, 1.0,
			1.0, false,true
		}
	},

		// Wave 2
		{
			// Ship 1
			{
				std::vector<std::string> { "Wave_1_Ship_1", "Wave_1_Ship_3" },
				6015, 0.0, 2.0, true, 0.0, 2.0,
				2.0, 1500, false, 0.0, 1.0,
				1.0, false, true
			},

		// Ship 2
		{
			std::vector<std::string> { "Wave_1_Ship_2", "Wave_1_Ship_4" },
			6300, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 500, false, 0.0, 1.0,
			1.0, false, true
		},

		// Ship 3
		{
			std::vector<std::string> { "Wave_2_Ship_1" },
			6300, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 500, false, 0.0, 1.0,
			1.0, false, true
		},

		// Ship 4
		{
			std::vector<std::string> { "Wave_2_Ship_2" },
			6015, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 500, false, 0.0, 1.0,
			1.0, false, true
		},

		// Sub 1
		{
			std::vector<std::string> { "Wave_1_Sub_1", "Wave_1_Sub_2" },
			6016, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 1000, false, 0.0, 1.0,
			1.0, true, true
		},

		// Sub 2
		//{
		//	std::vector<std::string> { "Wave_2_Sub_1", "Wave_2_Sub_2" },
		//	6016, 0.0, 2.0, true, 0.0, 2.0,
		//	2.0, 1000, false, 0.0, 1.0,
		//	1.0, true, true
		//},

		// Sub 2 but for dlu
		{
			std::vector<std::string> { "Wave_2_Sub_1", "Wave_2_Sub_2" },
			6016, 0.0, 2.0, true, 0.0, 2.0,
			3.0, 1000, false, 0.0, 1.0,
			1.0, true, true
		},

		// Duck
		{
			std::vector<std::string> { "Wave_1_Duck_1", "Wave_1_Duck_2" },
			5946, 5.0, 10.0, true, 5.0, 10.0,
			4.0, 5000, false, 0.0, 1.0,
			1.0, false, true
		},

		// Friendly
		{
			std::vector<std::string> { "Wave_3_FShip_1", "Wave_3_FShip_2" },
			2168,0.0,5.0,true, 2.0, 5.0,
			1.0, -1000, false, 0.0, 1.0,
			1.0, false,true
		}
	},

		// Wave 3
		{
			// Ship 1
			{
				std::vector<std::string> { "Wave_1_Ship_1", "Wave_1_Ship_3" },
				6015, 0.0, 2.0, true, 0.0, 2.0,
				2.0, 1500, false, 0.0, 1.0,
				1.0, false, true
			},

		// Ship 2
		{
			std::vector<std::string> { "Wave_1_Ship_2", "Wave_1_Ship_4" },
			6300, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 500, false, 0.0, 1.0,
			1.0, false, true
		},

		// Ship 3
		{
			std::vector<std::string> { "Wave_2_Ship_1", "Wave_2_Ship_2" },
			6015, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 500, false, 0.0, 1.0,
			1.0, false, true
		},

		// Ship 4
		{
			std::vector<std::string> { "Wave_3_Ship_1", "Wave_3_Ship_2" },
			6300, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 1500, false, 0.0, 1.0,
			1.0, false, true
		},

		// Sub 1
		{
			std::vector<std::string> { "Wave_1_Sub_1", "Wave_1_Sub_2" },
			6016, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 1000, false, 0.0, 1.0,
			1.0, true, true
		},

		// Sub 2
		//{
		//	std::vector<std::string> { "Wave_2_Sub_1", "Wave_2_Sub_2" },
		//	6016, 0.0, 2.0, true, 0.0, 2.0,
		//	2.0, 1000, false, 0.0, 1.0,
		//	1.0, true, true
		//},

		// Sub 2
		{
			std::vector<std::string> { "Wave_2_Sub_1", "Wave_2_Sub_2" },
			6016, 0.0, 2.0, true, 0.0, 2.0,
			3.0, 1000, false, 0.0, 1.0,
			1.0, true, true
		},

		// Sub 3
		{
			std::vector<std::string> { "Wave_3_Sub_1", "Wave_3_Sub_2" },
			6016, 0.0, 2.0, true, 0.0, 2.0,
			2.0, 1000, false, 0.0, 1.0,
			1.0, true, true
		},

		// Duck
		{
			std::vector<std::string> { "Wave_1_Duck_1", "Wave_1_Duck_2" },
			5946, 5.0, 10.0, true, 5.0, 10.0,
			4.0, 5000, false, 0.0, 1.0,
			1.0, false, true
		},

		// Ness temp fix for dlu where speeds are set to 7 to match a speed closer to live while we work on movingplatform components.
		{
			std::vector<std::string> { "Wave_1_Ness_1", "Wave_1_Ness_2", "Wave_2_Ness_1" },
			2565, 10.0, 15.0, true, 10.0, 15.0,
			7.0, 10000, false, 0.0, 7.0,
			7.0, true, true
		},

		// // Ness
		// {
		// 	std::vector<std::string> { "Wave_1_Ness_1", "Wave_1_Ness_2", "Wave_2_Ness_1" },
		// 	2565, 10.0, 15.0, true, 10.0, 15.0,
		// 	2.0, 10000, false, 0.0, 1.0,
		// 	1.0, true, true
		// },

		// Friendly 1
		{
			std::vector<std::string> { "Wave_3_FShip_1", "Wave_3_FShip_2" },
			2168,0.0,5.0,true, 2.0, 5.0,
			1.0, -1000, false, 0.0, 1.0,
			1.0, false,true
		},

		// Friendly 2
		{
			std::vector<std::string> { "Wave_3_FShip_1", "Wave_3_FShip_2" },
			2168,0.0,5.0,true, 2.0, 5.0,
			1.0, -1000, false, 0.0, 1.0,
			1.0, false,true
		}
	}
	};
}

void SGCannon::ResetVars(Entity* self) {
	self->SetVar<uint32_t>(SpawnNumberVariable, 0);
	self->SetVar<uint32_t>(CurrentSpawnNumberVariable, 0);
	self->SetVar<uint32_t>(ThisWaveVariable, 0);
	self->SetVar<uint32_t>(GameScoreVariable, 0);
	self->SetVar<uint32_t>(GameTimeVariable, 0);
	self->SetVar<bool>(GameStartedVariable, false);
	self->SetVar<uint32_t>(ShotsFiredVariable, 0);
	self->SetVar<uint32_t>(MaxStreakVariable, 0);
	self->SetVar<uint32_t>(MissesVariable, 0);
	self->SetVar<uint32_t>(CurrentStreakVariable, 0);
	self->SetVar<uint32_t>(CurrentSuperChargedTimeVariable, 0);
	self->SetVar<std::vector<uint32_t>>(StreakBonusVariable, {});
	self->SetVar<uint32_t>(LastSuperTotalVariable, 0);
	self->SetVar<LOT>(CurrentRewardVariable, LOT_NULL);
	self->SetVar<std::vector<LOT>>(RewardsVariable, {});
	self->SetVar<int32_t>(TotalScoreVariable, 0);

	self->SetVar<uint32_t>(u"m_curStreak", 0);
	self->SetVar<uint32_t>(u"LastSuperTotal", 0);
	self->SetVar<std::vector<LWOOBJID>>(u"LastHitTarget", {});

	const_cast<std::vector<SGEnemy>&>(self->GetVar<std::vector<SGEnemy>>(ActiveSpawnsVariable)).clear();
	self->SetVar<std::vector<SGEnemy>>(ActiveSpawnsVariable, {});

	const_cast<std::vector<LWOOBJID>&>(self->GetVar<std::vector<LWOOBJID>>(SpawnedObjects)).clear();
	self->SetVar<std::vector<LWOOBJID>>(SpawnedObjects, {});

	if (self->GetVar<bool>(InitVariable)) {
		ToggleSuperCharge(self, false);
	}

	self->SetVar<uint32_t>(ImpactSkillVariale, constants.impactSkillID);
	self->SetVar<std::vector<int32_t>>(PlayerScoresVariable, {});
	ActivityTimerStopAllTimers(self);
}

SGConstants SGCannon::GetConstants() {
	return {
		.playerStartPosition = Vector3 { -908.542480, 229.773178, -908.542480 },
		.playerStartRotation = Quaternion { 0.91913521289825, 0, 0.39394217729568, 0 },
		.cannonLot = 1864,
		.impactSkillID = 34,
		.projectileLot = 1822,
		.playerOffset = Vector3 { 6.652, -2, 1.5 },
		.rewardModelMatrix = 157,
		.cannonVelocity = 129.0,
		.cannonMinDistance = 30.0,
		.cannonRefireRate = 800.0,
		.cannonBarrelOffset = Vector3 { 0, 4.3, 9 },
		.cannonSuperchargedProjectileLot = 6297,
		.cannonProjectileLot = 1822,
		.cannonSuperChargeSkill = 249,
		.cannonSkill = 228,
		.cannonTimeout = -1,
		.cannonFOV = 58.6,
		.useLeaderboards = true,
		.streakModifier = 2,
		.chargedTime = 10,
		.chargedPoints = 25000,
		.rewardModelGroup = "QBRewardGroup",
		.activityID = 1864,
		.scoreRewards = {
			{"Score_Reward_1", 50000},
			{"Score_Reward_2", 100000},
			{"Score_Reward_3", 200000},
			{"Score_Reward_4", 400000},
			{"Score_Reward_5", 800000},
		},
		.scoreLootMatrix1 = 157,
		.scoreLootMatrix2 = 187,
		.scoreLootMatrix3 = 188,
		.scoreLootMatrix4 = 189,
		.scoreLootMatrix5 = 190,
		.firstWaveStartTime = 4.0,
		.inBetweenWavePause = 7.0
	};
}
