#include "BaseWavesServer.h"
#include "GameMessages.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Player.h"
#include "MissionTaskType.h"
#include "MissionComponent.h"
#include "Character.h"

// Done
void BaseWavesServer::SetGameVariables(Entity* self) {
	this->constants = std::move(GetConstants());
	this->waves = std::move(GetWaves());
	this->missions = std::move(GetWaveMissions());
	this->spawners = std::move(GetSpawnerNames());
}

// Done
void BaseWavesServer::BasePlayerLoaded(Entity* self, Entity* player) {
	GameMessages::SendPlayerSetCameraCyclingMode(player->GetObjectID(), player->GetSystemAddress());
	GameMessages::SendPlayerAllowedRespawn(player->GetObjectID(), true, player->GetSystemAddress());

	state.waitingPlayers.push_back(player->GetObjectID());
	state.players.push_back(player->GetObjectID());

	self->SetNetworkVar<uint32_t>(NumberOfPlayersVariable, self->GetNetworkVar<uint32_t>(NumberOfPlayersVariable) + 1);
	self->SetNetworkVar<std::string>(DefinePlayerToUIVariable, std::to_string(player->GetObjectID()), player->GetSystemAddress());

	// Notify the players of all other players
	if (!self->GetNetworkVar<bool>(WavesStartedVariable)) {
		auto counter = 1;
		for (const auto& playerID : state.players) {
			self->SetNetworkVar<std::string>(UpdateScoreboardPlayersVariable + GeneralUtils::to_u16string(counter), std::to_string(playerID));
			counter++;
		}

		if (!this->constants.introCelebration.empty()) {
			self->SetNetworkVar<std::string>(WatchingIntroVariable, this->constants.introCelebration + "_"
				+ std::to_string(player->GetObjectID()));
		} else {
			self->SetNetworkVar<bool>(ShowScoreboardVariable, true);
		}
	}

	SetPlayerSpawnPoints();

	if (!self->GetNetworkVar<bool>(WavesStartedVariable)) {
		PlayerConfirmed(self);
	} else {
		UpdatePlayer(self, player->GetObjectID());
		GetLeaderboardData(self, player->GetObjectID(), GetActivityID(self), 50);
		ResetStats(player->GetObjectID());
	}
}

// Done
void BaseWavesServer::BaseStartup(Entity* self) {
	self->SetVar<uint32_t>(PlayersAcceptedVariable, 0);
	self->SetVar<bool>(PlayersReadyVariable, false);
}

// Done
void BaseWavesServer::BasePlayerExit(Entity* self, Entity* player) {
	auto waitingPlayerToErase = std::find(state.waitingPlayers.begin(), state.waitingPlayers.end(), player->GetObjectID());
	if (waitingPlayerToErase != state.waitingPlayers.end()) state.waitingPlayers.erase(waitingPlayerToErase);

	auto playerToErase = std::find(state.players.begin(), state.players.end(), player->GetObjectID());
	if (playerToErase != state.players.end()) state.players.erase(playerToErase);

	if (!self->GetNetworkVar<bool>(WavesStartedVariable)) {
		PlayerConfirmed(self);

		if (state.players.empty())
			return;

		if (state.waitingPlayers.empty()) {
			ActivityTimerStopAllTimers(self);
			ActivityTimerStart(self, AllAcceptedDelayTimer, 1.0f, constants.startDelay);
		} else if (state.players.size() > state.waitingPlayers.size()) {
			if (!self->GetVar<bool>(AcceptedDelayStartedVariable)) {
				self->SetVar<bool>(AcceptedDelayStartedVariable, true);
				ActivityTimerStart(self, AcceptedDelayTimer, 1.0f, constants.acceptedDelay);
			}
		}
	} else {
		UpdatePlayer(self, player->GetObjectID(), true);
		if (CheckAllPlayersDead()) {
			GameOver(self);
		}
	}

	SetActivityValue(self, player->GetObjectID(), 1, 0);
	SetActivityValue(self, player->GetObjectID(), 2, 0);

	self->SetNetworkVar<uint32_t>(NumberOfPlayersVariable,
		std::min((uint32_t)0, self->GetNetworkVar<uint32_t>(NumberOfPlayersVariable) - 1));
}

// Done
void BaseWavesServer::BaseFireEvent(Entity* self, Entity* sender, const std::string& args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args == "start") {
		StartWaves(self);
	} else if (args == "Survival_Update") {
		const auto senderID = sender != nullptr ? sender->GetObjectID() : LWOOBJID_EMPTY;
		if (UpdateSpawnedEnemies(self, senderID, param1)) {
			const auto currentTime = GetActivityValue(self, senderID, 1);
			const auto currentWave = GetActivityValue(self, senderID, 2);

			for (const auto& mission : this->missions) {
				if (currentWave == mission.wave && currentTime <= mission.time) {
					UpdateMissionForAllPlayers(self, mission.missionID);
				}
			}
		}
	}
}

// Done
void BaseWavesServer::BasePlayerDied(Entity* self, Entity* player) {
	const auto currentTime = ActivityTimerGetCurrentTime(self, ClockTickTimer);
	const auto finalTime = GetActivityValue(self, player->GetObjectID(), 1);
	const auto finalWave = GetActivityValue(self, player->GetObjectID(), 2);

	auto paramString = CheckAllPlayersDead() ? "true" : "false";

	GameMessages::SendNotifyClientZoneObject(self->GetObjectID(), u"Player_Died", finalTime, finalWave,
		player->GetObjectID(), paramString, player->GetSystemAddress());

	if (!self->GetNetworkVar<bool>(WavesStartedVariable)) {
		player->Resurrect();
		return;
	}

	GameOver(self);
}

// Done
void BaseWavesServer::BasePlayerResurrected(Entity* self, Entity* player) {
	GameMessages::SendNotifyClientZoneObject(self->GetObjectID(), u"Player_Res", 0, 0,
		player->GetObjectID(), "", player->GetSystemAddress());

	if (self->GetNetworkVar<bool>(WavesStartedVariable))
		return;

	self->SetNetworkVar<bool>(ShowScoreboardVariable, true);
	SetPlayerSpawnPoints(player->GetObjectID());
}

// Done
void BaseWavesServer::BaseMessageBoxResponse(Entity* self, Entity* sender, int32_t button,
	const std::u16string& identifier, const std::u16string& userData) {
	if (identifier == u"RePlay") {
		PlayerAccepted(self, sender->GetObjectID());
		PlayerConfirmed(self);
	} else if (identifier == u"Exit_Question" && button == 1) {
		ResetStats(sender->GetObjectID());
		self->SetNetworkVar<std::string>(ExitWavesVariable, std::to_string(sender->GetObjectID()));

		if (sender->IsPlayer()) {
			auto* character = sender->GetCharacter();
			if (character != nullptr) {
				auto* player = dynamic_cast<Player*>(sender);
				player->SendToZone(character->GetLastNonInstanceZoneID());
			}
		}
	}
}

// Done
void BaseWavesServer::OnActivityTimerUpdate(Entity* self, const std::string& name, float_t remainingTime, float_t elapsedTime) {
	if (name == AcceptedDelayTimer) {
		self->SetNetworkVar<uint32_t>(UpdateDefaultStartTimerVariable, remainingTime);
	} else if (name == ClockTickTimer) {
		self->SetNetworkVar<float_t>(UpdateTimerVariable, elapsedTime);
	} else if (name == NextWaveTickTimer || name == TimedWaveTimer || name == GameOverWinTimer) {
		self->SetNetworkVar<uint32_t>(UpdateCooldownVariable, remainingTime);
	}
}

// Done
void BaseWavesServer::OnActivityTimerDone(Entity* self, const std::string& name) {
	if (name == AcceptedDelayTimer) {
		self->SetNetworkVar<uint32_t>(UpdateDefaultStartTimerVariable, 0);
		ActivityTimerStart(self, AllAcceptedDelayTimer, 1, 1);
	} else if (name == AllAcceptedDelayTimer) {
		self->SetNetworkVar<bool>(ClearScoreboardVariable, true);
		ActivityTimerStart(self, StartDelayTimer, 4, 4);
		StartWaves(self);
	} else if (name == StartDelayTimer) {
		ActivityTimerStart(self, ClockTickTimer, 1);
		SpawnWave(self);
		ActivityTimerStart(self, PlaySpawnSoundTimer, 3, 3);
	} else if (name == PlaySpawnSoundTimer) {
		for (const auto& playerID : state.players) {
			auto* player = EntityManager::Instance()->GetEntity(playerID);
			if (player != nullptr) {
				GameMessages::SendPlayNDAudioEmitter(player, player->GetSystemAddress(), spawnSoundGUID);
			}
		}
	} else if (name == NextWaveTickTimer) {
		self->SetNetworkVar<bool>(StartCooldownVariable, false);
		SpawnWave(self);
	} else if (name == WaveCompleteDelayTimer) {
		self->SetNetworkVar<uint32_t>(StartCooldownVariable, constants.waveTime);
		ActivityTimerStart(self, NextWaveTickTimer, 1, constants.waveTime);
	} else if (name == TimedWaveTimer) {
		ActivityTimerStart(self, WaveCompleteDelayTimer, constants.waveCompleteDelay, constants.waveCompleteDelay);

		const auto currentTime = ActivityTimerGetCurrentTime(self, ClockTickTimer);
		const auto currentWave = state.waveNumber;

		self->SetNetworkVar<uint32_t>(WaveCompleteVariable, { currentWave, (uint32_t)currentTime });
	} else if (name == GameOverWinTimer) {
		GameOver(self, true);
	} else if (name == CinematicDoneTimer) {
		for (auto* boss : EntityManager::Instance()->GetEntitiesInGroup("boss")) {
			boss->OnFireEventServerSide(self, "startAI");
		}
	}
}

// Done
void BaseWavesServer::ResetStats(LWOOBJID playerID) {
	auto* player = EntityManager::Instance()->GetEntity(playerID);
	if (player != nullptr) {

		// Boost all the player stats when loading in
		auto* destroyableComponent = player->GetComponent<DestroyableComponent>();
		if (destroyableComponent != nullptr) {
			destroyableComponent->SetHealth(destroyableComponent->GetMaxHealth());
			destroyableComponent->SetArmor(destroyableComponent->GetMaxArmor());
			destroyableComponent->SetImagination(destroyableComponent->GetMaxImagination());
		}
	}
}

// Done
void BaseWavesServer::PlayerConfirmed(Entity* self) {
	std::vector<LWOOBJID> confirmedPlayers{};

	for (const auto& playerID : state.players) {
		auto pass = false;
		for (const auto& waitingPlayerID : state.waitingPlayers) {
			if (waitingPlayerID == playerID)
				pass = true;
		}

		if (!pass)
			confirmedPlayers.push_back(playerID);
	}

	auto playerIndex = 1;
	for (const auto& playerID : confirmedPlayers) {
		self->SetNetworkVar<std::string>(PlayerConfirmVariable + GeneralUtils::to_u16string(playerIndex), std::to_string(playerID));
		playerIndex++;
	}
}

// Done
void BaseWavesServer::PlayerAccepted(Entity* self, LWOOBJID playerID) {
	state.waitingPlayers.erase(std::find(state.waitingPlayers.begin(), state.waitingPlayers.end(), playerID));
	if (state.waitingPlayers.empty() && state.players.size() >= self->GetNetworkVar<uint32_t>(NumberOfPlayersVariable)) {
		ActivityTimerStopAllTimers(self);
		ActivityTimerStart(self, AllAcceptedDelayTimer, 1, constants.startDelay);
	} else if (!self->GetVar<bool>(AcceptedDelayStartedVariable)) {
		self->SetVar<bool>(AcceptedDelayStartedVariable, true);
		ActivityTimerStart(self, AcceptedDelayTimer, 1, constants.acceptedDelay);
	}
}

// Done
void BaseWavesServer::StartWaves(Entity* self) {
	GameMessages::SendActivityStart(self->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);

	self->SetNetworkVar<std::string>(WatchingIntroVariable, "");
	self->SetVar<bool>(PlayersReadyVariable, true);
	self->SetVar<uint32_t>(BaseMobSetIndexVariable, 0);
	self->SetVar<uint32_t>(RandMobSetIndexVariable, 0);
	self->SetVar<bool>(AcceptedDelayStartedVariable, false);

	state.waitingPlayers.clear();

	for (const auto& playerID : state.players) {
		const auto player = EntityManager::Instance()->GetEntity(playerID);
		if (player != nullptr) {
			state.waitingPlayers.push_back(playerID);

			UpdatePlayer(self, playerID);
			GetLeaderboardData(self, playerID, GetActivityID(self), 1);
			ResetStats(playerID);

			if (!self->GetVar<bool>(FirstTimeDoneVariable)) {
				TakeActivityCost(self, playerID);
			}
		}
	}

	self->SetVar<bool>(FirstTimeDoneVariable, true);
	self->SetVar<std::string>(MissionTypeVariable, state.players.size() == 1 ? "survival_time_solo" : "survival_time_team");
	self->SetNetworkVar<bool>(WavesStartedVariable, true);
	self->SetNetworkVar<std::string>(StartWaveMessageVariable, "Start!");
}

// Done
bool BaseWavesServer::CheckAllPlayersDead() {
	auto deadPlayers = 0;

	for (const auto& playerID : state.players) {
		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player == nullptr || player->GetIsDead()) {
			deadPlayers++;
		}
	}

	return deadPlayers >= state.players.size();
}

// Done
void BaseWavesServer::SetPlayerSpawnPoints(const LWOOBJID& specificPlayerID) {
	auto spawnerIndex = 1;
	for (const auto& playerID : state.players) {
		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player != nullptr && (specificPlayerID == LWOOBJID_EMPTY || playerID == specificPlayerID)) {
			auto possibleSpawners = EntityManager::Instance()->GetEntitiesInGroup("P" + std::to_string(spawnerIndex) + "_Spawn");
			if (!possibleSpawners.empty()) {
				auto* spawner = possibleSpawners.at(0);
				GameMessages::SendTeleport(playerID, spawner->GetPosition(), spawner->GetRotation(), player->GetSystemAddress(), true);
			}
		}

		spawnerIndex++;
	}
}

// Done
void BaseWavesServer::GameOver(Entity* self, bool won) {
	if (!CheckAllPlayersDead() && !won)
		return;

	ActivityTimerStopAllTimers(self);

	// Reset all the spawners
	state.waveNumber = 0;
	state.totalSpawned = 0;
	state.currentSpawned = 0;

	self->SetNetworkVar<bool>(WavesStartedVariable, false);
	self->SetNetworkVar<uint32_t>(StartCooldownVariable, 0);
	SetPlayerSpawnPoints();
	ClearSpawners();

	for (const auto& playerID : state.players) {
		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player == nullptr)
			continue;

		const auto score = GetActivityValue(self, playerID, 0);
		const auto time = GetActivityValue(self, playerID, 1);
		const auto wave = GetActivityValue(self, playerID, 2);

		GameMessages::SendNotifyClientZoneObject(self->GetObjectID(), u"Update_ScoreBoard", time, 0,
			playerID, std::to_string(wave), UNASSIGNED_SYSTEM_ADDRESS);

		if (won) {
			SetPlayerSpawnPoints();
			self->SetNetworkVar<bool>(ShowScoreboardVariable, true);
		} else {
			player->Resurrect();
		}

		// Update all mission progression
		auto* missionComponent = player->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_MINIGAME, time, self->GetObjectID(), self->GetVar<std::string>(MissionTypeVariable));
		}

		StopActivity(self, playerID, wave, time, score);
	}
}

// Done
void BaseWavesServer::GameWon(Entity* self) {
	ActivityTimerStopAllTimers(self);

	const auto winDelay = waves.back().winDelay;
	ActivityTimerStart(self, GameOverWinTimer, 1, winDelay);
	self->SetNetworkVar<uint32_t>(StartTimedWaveVariable, { winDelay, state.waveNumber });
}

// Done
void BaseWavesServer::SpawnNow(const std::string& spawnerName, uint32_t amount, LOT spawnLot) {
	const auto spawners = dZoneManager::Instance()->GetSpawnersByName(spawnerName);
	for (auto* spawner : spawners) {
		if (spawnLot != LOT_NULL) {
			spawner->SetSpawnLot(spawnLot);
		}

		spawner->m_Info.amountMaintained = amount;
		spawner->m_Info.maxToSpawn = amount;

		spawner->Reset();
		spawner->Activate();
	}
}

// Done
void BaseWavesServer::SpawnWave(Entity* self) {
	if (!self->GetNetworkVar<bool>(WavesStartedVariable))
		return;

	// If there's no wave left
	if (state.waveNumber >= waves.size()) {
		GameOver(self);
		return;
	}

	const auto wave = waves.at(state.waveNumber);

	// Handles meta info to the client about the current round
	if (wave.winDelay != (uint32_t)-1) {
		self->SetNetworkVar<bool>(WonWaveVariable, true);

		// Close the game if we don't expect a notification from an other entity to end it
		if (!wave.notifyWin) {
			GameWon(self);
		}

		for (const auto& playerID : state.players) {
			auto* player = EntityManager::Instance()->GetEntity(playerID);
			if (player != nullptr) {
				player->Resurrect();
			}
		}
	} else {
		if (wave.timeLimit != (uint32_t)-1) {
			ActivityTimerStart(self, TimedWaveTimer, 1.0f, wave.timeLimit);
			self->SetNetworkVar<uint32_t>(StartTimedWaveVariable, { wave.timeLimit, state.waveNumber + 1 });
		} else {
			self->SetNetworkVar<uint32_t>(NewWaveVariable, state.waveNumber + 1);
		}
	}

	// NOTE: The script does some stuff with events here, although BONS does not have those

	// Optional cinematics to play
	if (!wave.cinematic.empty()) {
		ActivityTimerStart(self, CinematicDoneTimer, wave.cinematicLength, wave.cinematicLength);
		self->SetNetworkVar<std::string>(StartCinematicVariable, wave.cinematic);
	}

	// Spawn the enemies
	state.currentSpawned = 0;

	for (const auto& mobDefinition : wave.waveMobs) {
		SpawnNow(mobDefinition.spawnerName, mobDefinition.amountToSpawn, mobDefinition.lot);
		state.currentSpawned += mobDefinition.amountToSpawn;
	}

	state.waveNumber++;
	state.totalSpawned += state.currentSpawned;
	self->SetNetworkVar<uint32_t>(NumRemainingVariable, state.currentSpawned);
}

// Done
bool BaseWavesServer::UpdateSpawnedEnemies(Entity* self, LWOOBJID enemyID, uint32_t score) {
	if (!self->GetNetworkVar<bool>(WavesStartedVariable))
		return false;

	state.currentSpawned--;

	auto* enemy = EntityManager::Instance()->GetEntity(enemyID);
	if (enemy != nullptr && enemy->IsPlayer() && IsPlayerInActivity(self, enemyID)) {
		SetActivityValue(self, enemyID, 0, GetActivityValue(self, enemyID, 0) + score);
	}

	if (state.currentSpawned <= 0) {
		const auto currentTime = ActivityTimerGetCurrentTime(self, ClockTickTimer);
		const auto completedWave = state.waveNumber - 1;

		// When the last enemy is smashed (e.g. in last wave - 1)
		if (state.waveNumber >= waves.size() - 1) {

			// If there's no more follow up waves, (e.g in last wave), end the game. Generally called by some other script
			if (state.waveNumber >= waves.size()) {
				GameWon(self);
				return false;
			}

			ActivityTimerStopAllTimers(self);
			self->SetNetworkVar<float_t>(UpdateTimerVariable, currentTime);
		}

		ActivityTimerStart(self, WaveCompleteDelayTimer, constants.waveCompleteDelay, constants.waveCompleteDelay);

		const auto waveMission = waves.at(completedWave).missions;
		const auto soloWaveMissions = waves.at(completedWave).soloMissions;

		for (const auto& playerID : state.players) {
			auto* player = EntityManager::Instance()->GetEntity(playerID);
			if (player != nullptr && !player->GetIsDead()) {
				SetActivityValue(self, playerID, 1, currentTime);
				SetActivityValue(self, playerID, 2, state.waveNumber);

				// Update player missions
				auto* missionComponent = player->GetComponent<MissionComponent>();
				if (missionComponent != nullptr) {
					for (const auto& missionID : waveMission) {
						// Get the mission state
						auto missionState = missionComponent->GetMissionState(missionID);
						// For some reason these achievements are not accepted by default, so we accept them here if they arent already.
						if (missionState != MissionState::MISSION_STATE_COMPLETE && missionState != MissionState::MISSION_STATE_UNKNOWN) {
							missionComponent->AcceptMission(missionID);
							missionState = missionComponent->GetMissionState(missionID);
						}

						if (missionState != MissionState::MISSION_STATE_COMPLETE) {
							auto mission = missionComponent->GetMission(missionID);
							if (mission != nullptr) {
								mission->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
							}
						}
					}
					// Progress solo missions
					if (state.players.size() == 1) {
						for (const auto& missionID : soloWaveMissions) {
							// Get the mission state
							auto missionState = missionComponent->GetMissionState(missionID);
							// For some reason these achievements are not accepted by default, so we accept them here if they arent already.
							if (missionState != MissionState::MISSION_STATE_COMPLETE && missionState != MissionState::MISSION_STATE_UNKNOWN) {
								missionComponent->AcceptMission(missionID);
								missionState = missionComponent->GetMissionState(missionID);
							}

							if (missionState != MissionState::MISSION_STATE_COMPLETE) {
								auto mission = missionComponent->GetMission(missionID);
								if (mission != nullptr) {
									mission->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
								}
							}
						}
					}
				}
			}
		}

		// Might seem odd to send the next wave but the client isn't 0-indexed so it thinks it completed the correct wave
		self->SetNetworkVar<uint32_t>(WaveCompleteVariable, { state.waveNumber, (uint32_t)currentTime });
		return true;
	}

	self->SetNetworkVar<uint32_t>(NumRemainingVariable, state.currentSpawned);
	return false;
}

// Done
void BaseWavesServer::UpdateMissionForAllPlayers(Entity* self, uint32_t missionID) {
	for (const auto& playerID : state.players) {
		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player != nullptr) {
			auto* missionComponent = player->GetComponent<MissionComponent>();
			if (missionComponent == nullptr) return;
			// Get the mission state
			auto missionState = missionComponent->GetMissionState(missionID);
			// For some reason these achievements are not accepted by default, so we accept them here if they arent already.
			if (missionState != MissionState::MISSION_STATE_COMPLETE && missionState != MissionState::MISSION_STATE_UNKNOWN) {
				missionComponent->AcceptMission(missionID);
				missionState = missionComponent->GetMissionState(missionID);
			}
			if (missionState != MissionState::MISSION_STATE_COMPLETE) {
				auto mission = missionComponent->GetMission(missionID);
				if (mission != nullptr) {
					mission->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
				}
			}
		}
	}
}

void BaseWavesServer::ClearSpawners() {
	for (const auto& spawnerName : spawners) {
		const auto spawnerObjects = dZoneManager::Instance()->GetSpawnersByName(spawnerName);

		for (auto* spawnerObject : spawnerObjects) {
			spawnerObject->Reset();
			spawnerObject->Deactivate();
		}
	}
}
