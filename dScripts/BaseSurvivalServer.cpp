#include "BaseSurvivalServer.h"
#include "GameMessages.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Player.h"
#include "MissionTaskType.h"
#include "MissionComponent.h"
#include "Character.h"

void BaseSurvivalServer::SetGameVariables(Entity* self) {
	this->constants = std::move(GetConstants());
	this->mobSets = std::move(GetMobSets());
	this->spawnerNetworks = std::move(GetSpawnerNetworks());
	this->missionsToUpdate = std::move(GetMissionsToUpdate());
}

void BaseSurvivalServer::BasePlayerLoaded(Entity* self, Entity* player) {
	const auto& waitingIter = std::find(state.waitingPlayers.begin(), state.waitingPlayers.end(), player->GetObjectID());
	const auto& playersIter = std::find(state.players.begin(), state.players.end(), player->GetObjectID());

	if (waitingIter != state.waitingPlayers.end() || playersIter != state.players.end()) {
		static_cast<Player*>(player)->SendToZone(player->GetCharacter()->GetLastNonInstanceZoneID());

		return;
	}

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

		self->SetNetworkVar<bool>(ShowScoreboardVariable, true);
	}

	SetPlayerSpawnPoints();

	if (!self->GetNetworkVar<bool>(WavesStartedVariable)) {
		PlayerConfirmed(self);
	} else {
		UpdatePlayer(self, player->GetObjectID());
		GetLeaderboardData(self, player->GetObjectID(), GetActivityID(self), 50);
		ResetStats(player->GetObjectID());
	}

	player->AddCallbackTimer(5.0f, [this, self, player]() {
		self->SetNetworkVar<uint32_t>(NumberOfPlayersVariable, self->GetNetworkVar<uint32_t>(NumberOfPlayersVariable));
		self->SetNetworkVar<std::string>(DefinePlayerToUIVariable, std::to_string(player->GetObjectID()), player->GetSystemAddress());
		if (!self->GetNetworkVar<bool>(WavesStartedVariable)) {
			auto counter = 1;
			for (const auto& playerID : state.players) {
				self->SetNetworkVar<std::string>(UpdateScoreboardPlayersVariable + GeneralUtils::to_u16string(counter), std::to_string(playerID));
				counter++;
			}

			self->SetNetworkVar<bool>(ShowScoreboardVariable, true);
		}
		});
}

void BaseSurvivalServer::BaseStartup(Entity* self) {
	self->SetVar<uint32_t>(PlayersAcceptedVariable, 0);
	self->SetVar<bool>(PlayersReadyVariable, false);
}

void BaseSurvivalServer::BasePlayerExit(Entity* self, Entity* player) {
	const auto& waitingIter = std::find(state.waitingPlayers.begin(), state.waitingPlayers.end(), player->GetObjectID());
	const auto& playersIter = std::find(state.players.begin(), state.players.end(), player->GetObjectID());

	if (waitingIter != state.waitingPlayers.end()) {
		state.waitingPlayers.erase(waitingIter);
	}

	if (playersIter != state.players.end()) {
		state.players.erase(playersIter);
	}

	if (waitingIter == state.waitingPlayers.end() && playersIter == state.players.end()) {
		return;
	}

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
	self->SetNetworkVar<uint32_t>(NumberOfPlayersVariable,
		std::min((uint32_t)0, self->GetNetworkVar<uint32_t>(NumberOfPlayersVariable) - 1));
}

void BaseSurvivalServer::BaseFireEvent(Entity* self, Entity* sender, const std::string& args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args == "start") {
		StartWaves(self);
	} else if (args == "DeactivateRewards") {
		SpawnerReset(spawnerNetworks.rewardNetworks);
	} else if (sender != nullptr && IsPlayerInActivity(self, sender->GetObjectID())) {
		auto currentScore = GetActivityValue(self, sender->GetObjectID(), 0);
		SetActivityValue(self, sender->GetObjectID(), 0, currentScore + param1);
	}
}

void BaseSurvivalServer::BasePlayerDied(Entity* self, Entity* player) {
	if (self->GetNetworkVar<bool>(WavesStartedVariable)) {
		const auto finalTime = ActivityTimerGetCurrentTime(self, ClockTickTimer);
		SetActivityValue(self, player->GetObjectID(), 1, finalTime);

		auto paramString = CheckAllPlayersDead() ? "true" : "false";
		GameMessages::SendNotifyClientZoneObject(self->GetObjectID(), u"Player_Died", finalTime, 0,
			player->GetObjectID(), paramString, player->GetSystemAddress());
		GameOver(self);
	} else {
		player->Resurrect();
		SetPlayerSpawnPoints();
	}
}

void BaseSurvivalServer::BasePlayerResurrected(Entity* self, Entity* player) {
	self->SetNetworkVar<bool>(ShowScoreboardVariable, true);
}

void BaseSurvivalServer::BaseMessageBoxResponse(Entity* self, Entity* sender, int32_t button,
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

void BaseSurvivalServer::OnActivityTimerUpdate(Entity* self, const std::string& name, float_t remainingTime, float_t elapsedTime) {
	if (name == AcceptedDelayTimer) {
		self->SetNetworkVar<uint64_t>(UpdateDefaultStartTimerVariable, remainingTime);
	} else if (name == ClockTickTimer) {
		self->SetNetworkVar<float_t>(UpdateTimerVariable, elapsedTime);
	} else if (name == SpawnTickTimer && !self->GetVar<bool>(IsCooldownVariable)) {
		SpawnMobs(self);
	}
}

void BaseSurvivalServer::OnActivityTimerDone(Entity* self, const std::string& name) {
	auto cooldownTime = constants.rewardInterval * constants.waveTime;

	if (name == AcceptedDelayTimer) {
		self->SetNetworkVar<uint64_t>(UpdateDefaultStartTimerVariable, 0);
		ActivityTimerStart(self, AllAcceptedDelayTimer, 1, 1);
	} else if (name == AllAcceptedDelayTimer) {
		self->SetNetworkVar<bool>(ClearScoreboardVariable, true);
		ActivityTimerStart(self, StartDelayTimer, 3, 3);

		StartWaves(self);
	} else if (name == StartDelayTimer) {
		ActivityTimerStart(self, ClockTickTimer, 1);
		ActivityTimerStart(self, SpawnTickTimer, constants.waveTime);
		SpawnMobs(self);
		ActivityTimerStart(self, PlaySpawnSoundTimer, 3, 3);
		ActivityTimerStart(self, CoolDownStartTimer, cooldownTime, cooldownTime);
	} else if (name == CoolDownStartTimer) {
		self->SetVar<bool>(IsCooldownVariable, true);

		ActivityTimerStop(self, SpawnTickTimer);
		ActivityTimerStart(self, CoolDownStopTimer, 1, constants.coolDownTime);

		ActivateSpawnerNetwork(spawnerNetworks.rewardNetworks);
		SpawnerReset(spawnerNetworks.baseNetworks, false);
		SpawnerReset(spawnerNetworks.randomNetworks, false);
	} else if (name == CoolDownStopTimer) {
		self->SetVar<bool>(IsCooldownVariable, false);

		ActivityTimerStart(self, SpawnTickTimer, constants.waveTime);
		ActivityTimerStart(self, CoolDownStartTimer, cooldownTime, cooldownTime);

		SpawnMobs(self);
		ActivityTimerStart(self, PlaySpawnSoundTimer, 3, 3);
	} else if (name == PlaySpawnSoundTimer) {
		for (const auto& playerID : state.players) {
			auto* player = EntityManager::Instance()->GetEntity(playerID);
			if (player != nullptr) {
				GameMessages::SendPlayNDAudioEmitter(player, player->GetSystemAddress(), spawnSoundGUID);
			}
		}
	}
}

void BaseSurvivalServer::ResetStats(LWOOBJID playerID) {
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

void BaseSurvivalServer::PlayerConfirmed(Entity* self) {
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

void BaseSurvivalServer::PlayerAccepted(Entity* self, LWOOBJID playerID) {
	const auto& iter = std::find(state.waitingPlayers.begin(), state.waitingPlayers.end(), playerID);

	if (iter == state.waitingPlayers.end()) {
		return;
	}

	state.waitingPlayers.erase(iter);

	if (state.waitingPlayers.empty() && state.players.size() >= self->GetNetworkVar<uint32_t>(NumberOfPlayersVariable)) {
		ActivityTimerStopAllTimers(self);
		ActivityTimerStart(self, AllAcceptedDelayTimer, 1, constants.startDelay);
	} else if (!self->GetVar<bool>(AcceptedDelayStartedVariable)) {
		self->SetVar<bool>(AcceptedDelayStartedVariable, true);
		ActivityTimerStart(self, AcceptedDelayTimer, 1, constants.acceptedDelay);
	}
}

void BaseSurvivalServer::StartWaves(Entity* self) {
	GameMessages::SendActivityStart(self->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);

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
			GetLeaderboardData(self, playerID, GetActivityID(self), 50);
			ResetStats(playerID);

			if (!self->GetVar<bool>(FirstTimeDoneVariable)) {
				TakeActivityCost(self, playerID);
			}

			GameMessages::SendPlayerSetCameraCyclingMode(playerID, player->GetSystemAddress());
		}
	}

	self->SetVar<bool>(FirstTimeDoneVariable, true);
	self->SetVar<std::string>(MissionTypeVariable, state.players.size() == 1 ? "survival_time_solo" : "survival_time_team");

	ActivateSpawnerNetwork(spawnerNetworks.smashNetworks);
	self->SetNetworkVar<bool>(WavesStartedVariable, true);
	self->SetNetworkVar<std::string>(StartWaveMessageVariable, "Start!");
}

bool BaseSurvivalServer::CheckAllPlayersDead() {
	auto deadPlayers = 0;

	for (const auto& playerID : state.players) {
		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player == nullptr || player->GetIsDead()) {
			deadPlayers++;
		}
	}

	return deadPlayers >= state.players.size();
}

void BaseSurvivalServer::SetPlayerSpawnPoints() {
	auto spawnerIndex = 1;
	for (const auto& playerID : state.players) {
		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player != nullptr) {
			auto possibleSpawners = EntityManager::Instance()->GetEntitiesInGroup("P" + std::to_string(spawnerIndex) + "_Spawn");
			if (!possibleSpawners.empty()) {
				auto* spawner = possibleSpawners.at(0);
				GameMessages::SendTeleport(playerID, spawner->GetPosition(), spawner->GetRotation(), player->GetSystemAddress(), true);
			}
		}

		spawnerIndex++;
	}
}

void BaseSurvivalServer::GameOver(Entity* self) {
	if (!CheckAllPlayersDead())
		return;

	ActivityTimerStopAllTimers(self);

	// Reset all the spawners
	SpawnerReset(spawnerNetworks.baseNetworks);
	SpawnerReset(spawnerNetworks.randomNetworks);
	SpawnerReset(spawnerNetworks.rewardNetworks);

	for (const auto& playerID : state.players) {
		auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player == nullptr)
			continue;

		const auto score = GetActivityValue(self, playerID, 0);
		const auto time = GetActivityValue(self, playerID, 1);

		GameMessages::SendNotifyClientZoneObject(self->GetObjectID(), u"Update_ScoreBoard", time, 0,
			playerID, std::to_string(score), UNASSIGNED_SYSTEM_ADDRESS);
		player->Resurrect();

		// Update all mission progression
		auto* missionComponent = player->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_MINIGAME, time, self->GetObjectID(),
				self->GetVar<std::string>(MissionTypeVariable));

			for (const auto& survivalMission : missionsToUpdate) {
				auto* mission = missionComponent->GetMission(survivalMission.first);
				if (mission != nullptr && (uint32_t)time >= survivalMission.second
					&& (mission->GetMissionState() == MissionState::MISSION_STATE_ACTIVE
						|| mission->GetMissionState() == MissionState::MISSION_STATE_COMPLETE_ACTIVE)) {

					mission->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
				}
			}
		}

		StopActivity(self, playerID, score, time);
	}

	state.waveNumber = 1;
	state.rewardTick = 1;
	state.totalSpawned = 0;

	self->SetNetworkVar<bool>(WavesStartedVariable, false);

	if (constants.useMobLots) {
		constants.lotPhase = 0;
		UpdateMobLots(spawnerNetworks.baseNetworks);
		UpdateMobLots(spawnerNetworks.randomNetworks);
	}

	SetPlayerSpawnPoints();
}

void BaseSurvivalServer::SpawnerReset(SpawnerNetworkCollection& spawnerNetworkCollection, bool hardReset) {
	auto totalSpawned = 0;

	for (auto& spawner : spawnerNetworkCollection.networks) {
		for (auto& spawnerName : spawner.names) {
			auto spawners = dZoneManager::Instance()->GetSpawnersByName(spawnerName + spawner.number);
			if (!spawners.empty()) {
				auto* spawnerObject = spawners.at(0);

				auto amountSpawned = spawnerObject->GetAmountSpawned();
				totalSpawned += amountSpawned;

				spawner.isActive = false;
				if (hardReset) {
					spawnerObject->Reset();
				} else {
					spawnerObject->SoftReset();
				}

				spawnerObject->Deactivate();
			}
		}
	}

	state.totalSpawned = std::max((uint32_t)totalSpawned, state.totalSpawned);
}

void BaseSurvivalServer::SpawnerUpdate(Entity* self, SpawnerNetworkCollection& spawnerNetworkCollection, uint32_t amount) {
	if (spawnerNetworkCollection.networks.empty())
		return;

	// If we want to spawn something specific now
	if (amount != 0) {
		auto spawnerNetwork = spawnerNetworkCollection.networks.at(0);
		auto possibleSpawners = dZoneManager::Instance()->GetSpawnersByName(spawnerNetwork.names.at(0) + spawnerNetwork.number);
		if (!possibleSpawners.empty()) {
			SpawnNow(possibleSpawners.at(0), amount);
			return;
		}
	}

	auto setNumber = self->GetVar<uint32_t>(GeneralUtils::ASCIIToUTF16(spawnerNetworkCollection.mobSetName + "Num"));
	auto newSet = GetRandomMobSet(spawnerNetworkCollection, setNumber);

	if (!newSet.empty()) {
		auto spawnerNetwork = GetRandomSpawner(spawnerNetworkCollection);
		for (auto i = 0; i < spawnerNetwork.names.size(); i++) {
			const auto& name = spawnerNetwork.names.at(i);
			const auto& toSpawn = newSet.at(i);

			auto possibleSpawners = dZoneManager::Instance()->GetSpawnersByName(name + spawnerNetwork.number);
			if (!possibleSpawners.empty()) {
				SpawnNow(possibleSpawners.front(), toSpawn);
			}
		}
	}
}

void BaseSurvivalServer::SpawnNow(Spawner* spawner, uint32_t amount) {
	if (spawner != nullptr) {
		if (!spawner->m_Active) {
			spawner->m_Info.amountMaintained = amount;
			spawner->Activate();
		} else {
			spawner->m_Info.amountMaintained = amount;
		}
	}
}

std::vector<uint32_t> BaseSurvivalServer::GetRandomMobSet(SpawnerNetworkCollection& spawnerNetworkCollection,
	uint32_t setNumber) {

	if (mobSets.sets.find(spawnerNetworkCollection.mobSetName) != mobSets.sets.end()) {
		auto mobSet = mobSets.sets.at(spawnerNetworkCollection.mobSetName);
		if (setNumber < mobSet.size()) {
			return mobSet.at(setNumber).at(rand() % mobSet.at(setNumber).size());
		}
	}

	return {};
}

SpawnerNetwork BaseSurvivalServer::GetRandomSpawner(SpawnerNetworkCollection& spawnerNetworkCollection) {
	std::vector<SpawnerNetwork> validSpawners{};
	for (const auto& spawner : spawnerNetworkCollection.networks) {
		if (!spawner.isLocked)
			validSpawners.push_back(spawner);
	}

	if (!validSpawners.empty()) {
		auto spawner = validSpawners.at(rand() % validSpawners.size());
		spawner.isActive = true;
		return spawner;
	}

	return {};
}

void BaseSurvivalServer::ActivateSpawnerNetwork(SpawnerNetworkCollection& spawnerNetworkCollection) {
	for (auto& spawner : spawnerNetworkCollection.networks) {
		for (const auto& spawnerName : spawner.names) {
			auto possibleSpawners = dZoneManager::Instance()->GetSpawnersByName(spawnerName + spawner.number);
			if (!possibleSpawners.empty()) {
				auto* spawnerObject = possibleSpawners.at(0);
				spawnerObject->Activate();
				spawnerObject->Reset();
			}
		}
	}
}

void BaseSurvivalServer::UpdateMobLots(SpawnerNetworkCollection& spawnerNetworkCollection) {
	for (auto& spawner : spawnerNetworkCollection.networks) {
		for (auto& spawnerName : spawner.names) {
			if (!spawnerName.empty()) {
				auto spawnerObjects = dZoneManager::Instance()->GetSpawnersByName(spawnerName + spawner.number);
				if (!spawnerObjects.empty()) {
					auto splitName = GeneralUtils::SplitString(spawnerName, '_');
					auto cleanName = splitName.size() > 1 ? splitName.at(1) : splitName.at(0);

					if (!cleanName.empty()) {
						auto spawnerObject = spawnerObjects.at(0);
						spawnerObject->SetSpawnLot(mobSets.mobLots.at(cleanName).at(constants.lotPhase));
					}
				}
			}
		}
	}
}

void BaseSurvivalServer::SpawnMobs(Entity* self) {
	if (!self->GetNetworkVar<bool>(WavesStartedVariable))
		return;

	state.waveNumber++;
	auto spawnNumber = state.waveNumber > constants.rewardInterval ? state.waveNumber - state.rewardTick - 1 : state.waveNumber;

	for (const auto& tier : constants.baseMobsStartTier) {
		if (tier == spawnNumber)
			self->SetVar<uint32_t>(BaseMobSetIndexVariable, (self->GetVar<uint32_t>(BaseMobSetIndexVariable) + 1)
				% (constants.baseMobsStartTier.size() - 1));
	}

	for (const auto& tier : constants.randMobsStartTier) {
		if (tier == spawnNumber)
			self->SetVar<uint32_t>(RandMobSetIndexVariable, (self->GetVar<uint32_t>(RandMobSetIndexVariable) + 1)
				% (constants.randMobsStartTier.size() - 1));
	}

	if (state.waveNumber == constants.unlockNetwork3)
		spawnerNetworks.randomNetworks.networks.at(2).isLocked = false;

	SpawnerReset(spawnerNetworks.baseNetworks, false);
	SpawnerReset(spawnerNetworks.randomNetworks, false);
	SpawnerUpdate(self, spawnerNetworks.baseNetworks);

	if (spawnNumber >= constants.mobSet2Wave) {
		if (spawnNumber == constants.mobSet2Wave)
			self->SetNetworkVar<std::string>(SpawnMobVariable, "2");
		SpawnerUpdate(self, spawnerNetworks.randomNetworks);
	}

	if (spawnNumber >= constants.mobSet3Wave) {
		if (spawnNumber == constants.mobSet3Wave)
			self->SetNetworkVar<std::string>(SpawnMobVariable, "3");
		SpawnerUpdate(self, spawnerNetworks.randomNetworks);
	}

	// If we reached the end of the spawn phase we increase the lost to make it more difficult
	if (constants.useMobLots && constants.lotPhase < (mobSets.mobLots.begin()->second.size() - 1)
		&& spawnNumber >= constants.baseMobsStartTier.back()) {
		state.waveNumber = 1;
		constants.lotPhase++;

		UpdateMobLots(spawnerNetworks.baseNetworks);
		UpdateMobLots(spawnerNetworks.randomNetworks);
	}
}
