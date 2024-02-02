#include "ActivityComponent.h"
#include "GameMessages.h"
#include "CDClientManager.h"
#include "MissionComponent.h"
#include "Character.h"
#include "dZoneManager.h"
#include "ZoneInstanceManager.h"
#include "Game.h"
#include "Logger.h"
#include "WorldPackets.h"
#include "EntityManager.h"
#include "ChatPackets.h"
#include "Player.h"
#include "BitStreamUtils.h"
#include "dServer.h"
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "dConfig.h"
#include "InventoryComponent.h"
#include "DestroyableComponent.h"
#include "Loot.h"
#include "eMissionTaskType.h"
#include "eMatchUpdate.h"
#include "eConnectionType.h"
#include "eChatInternalMessageType.h"

#include "CDCurrencyTableTable.h"
#include "CDActivityRewardsTable.h"
#include "CDActivitiesTable.h"
#include "LeaderboardManager.h"

ActivityComponent::ActivityComponent(Entity* parent, int32_t activityID) : Component(parent) {
	/*
	* This is precisely what the client does functionally
	* Use the component id as the default activity id and load its data from the database
	* if activityID is specified and if that column exists in the activities table, update the activity info with that data.
	*/

	m_ActivityID = activityID;
	LoadActivityData(activityID);
	if (m_Parent->HasVar(u"activityID")) {
		m_ActivityID = parent->GetVar<int32_t>(u"activityID");
		LoadActivityData(m_ActivityID);
	}

	auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();

	if (destroyableComponent) {
		// First lookup the loot matrix id for this component id.
		CDActivityRewardsTable* activityRewardsTable = CDClientManager::Instance().GetTable<CDActivityRewardsTable>();
		std::vector<CDActivityRewards> activityRewards = activityRewardsTable->Query([=](CDActivityRewards entry) {return (entry.LootMatrixIndex == destroyableComponent->GetLootMatrixID()); });

		uint32_t startingLMI = 0;

		// If we have one, set the starting loot matrix id to that.
		if (activityRewards.size() > 0) {
			startingLMI = activityRewards[0].LootMatrixIndex;
		}

		if (startingLMI > 0) {
			// We may have more than 1 loot matrix index to use depending ont the size of the team that is looting the activity.
			// So this logic will get the rest of the loot matrix indices for this activity.

			std::vector<CDActivityRewards> objectTemplateActivities = activityRewardsTable->Query([=](CDActivityRewards entry) {return (activityRewards[0].objectTemplate == entry.objectTemplate); });
			for (const auto& item : objectTemplateActivities) {
				if (item.activityRating > 0 && item.activityRating < 5) {
					m_ActivityLootMatrices.insert({ item.activityRating, item.LootMatrixIndex });
				}
			}
		}
	}
}
void ActivityComponent::LoadActivityData(const int32_t activityId) {
	CDActivitiesTable* activitiesTable = CDClientManager::Instance().GetTable<CDActivitiesTable>();
	std::vector<CDActivities> activities = activitiesTable->Query([activityId](CDActivities entry) {return (entry.ActivityID == activityId); });

	bool soloRacing = Game::config->GetValue("solo_racing") == "1";
	for (CDActivities activity : activities) {
		m_ActivityInfo = activity;
		if (static_cast<Leaderboard::Type>(activity.leaderboardType) == Leaderboard::Type::Racing && soloRacing) {
			m_ActivityInfo.minTeamSize = 1;
			m_ActivityInfo.minTeams = 1;
		}
		if (m_ActivityInfo.instanceMapID == -1) {
			const auto& transferOverride = m_Parent->GetVarAsString(u"transferZoneID");
			if (!transferOverride.empty()) {
				m_ActivityInfo.instanceMapID =
					GeneralUtils::TryParse<uint32_t>(transferOverride).value_or(m_ActivityInfo.instanceMapID);
			}
		}
	}
}

void ActivityComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	outBitStream->Write(m_DirtyActivityInfo);
	if (m_DirtyActivityInfo) {
		outBitStream->Write<uint32_t>(m_ActivityPlayers.size());
		if (!m_ActivityPlayers.empty()) {
			for (const auto& activityPlayer : m_ActivityPlayers) {
				outBitStream->Write<LWOOBJID>(activityPlayer->playerID);
				for (const auto& activityValue : activityPlayer->values) {
					outBitStream->Write<float_t>(activityValue);
				}
			}
		}
		if (!bIsInitialUpdate) m_DirtyActivityInfo = false;
	}
}

void ActivityComponent::ReloadConfig() {
	CDActivitiesTable* activitiesTable = CDClientManager::Instance().GetTable<CDActivitiesTable>();
	std::vector<CDActivities> activities = activitiesTable->Query([this](CDActivities entry) {return (entry.ActivityID == m_ActivityID); });
	for (auto activity : activities) {
		auto mapID = m_ActivityInfo.instanceMapID;
		if (static_cast<Leaderboard::Type>(activity.leaderboardType) == Leaderboard::Type::Racing && Game::config->GetValue("solo_racing") == "1") {
			m_ActivityInfo.minTeamSize = 1;
			m_ActivityInfo.minTeams = 1;
		} else {
			m_ActivityInfo.minTeamSize = activity.minTeamSize;
			m_ActivityInfo.minTeams = activity.minTeams;
		}
	}
}

void ActivityComponent::HandleMessageBoxResponse(Entity* player, const std::string& id) {
	if (id == "LobbyExit") {
		PlayerLeave(player->GetObjectID());
	} else if (id == "PlayButton") {
		PlayerJoin(player);
	}
}

void ActivityComponent::PlayerJoin(Entity* player) {
	if (PlayerIsInQueue(player)) return;
	// If we have a lobby, queue the player and allow others to join, otherwise spin up an instance on the spot
	if (HasLobby()) {
		PlayerJoinLobby(player);
	} else if (!IsPlayedBy(player)) {
		auto* instance = NewInstance();
		instance->AddParticipant(player);
	}
}

void ActivityComponent::PlayerJoinLobby(Entity* player) {
	if (!m_Parent->HasComponent(eReplicaComponentType::QUICK_BUILD))
		GameMessages::SendMatchResponse(player, player->GetSystemAddress(), 0); // tell the client they joined a lobby
	LobbyPlayer* newLobbyPlayer = new LobbyPlayer();
	newLobbyPlayer->entityID = player->GetObjectID();
	Lobby* playerLobby = nullptr;

	auto* character = player->GetCharacter();
	if (character != nullptr)
		character->SetLastNonInstanceZoneID(Game::zoneManager->GetZone()->GetWorldID());

	for (Lobby* lobby : m_Queue) {
		if (lobby->players.size() < m_ActivityInfo.maxTeamSize || m_ActivityInfo.maxTeamSize == 1 && lobby->players.size() < m_ActivityInfo.maxTeams) {
			// If an empty slot in an existing lobby is found
			lobby->players.push_back(newLobbyPlayer);
			playerLobby = lobby;

			// Update the joining player on players already in the lobby, and update players already in the lobby on the joining player
			std::string matchUpdateJoined = "player=9:" + std::to_string(player->GetObjectID()) + "\nplayerName=0:" + player->GetCharacter()->GetName();
			for (LobbyPlayer* joinedPlayer : lobby->players) {
				auto* entity = joinedPlayer->GetEntity();

				if (entity == nullptr) {
					continue;
				}

				std::string matchUpdate = "player=9:" + std::to_string(entity->GetObjectID()) + "\nplayerName=0:" + entity->GetCharacter()->GetName();
				GameMessages::SendMatchUpdate(player, player->GetSystemAddress(), matchUpdate, eMatchUpdate::PLAYER_ADDED);
				PlayerReady(entity, joinedPlayer->ready);
				GameMessages::SendMatchUpdate(entity, entity->GetSystemAddress(), matchUpdateJoined, eMatchUpdate::PLAYER_ADDED);
			}
		}
	}

	if (!playerLobby) {
		// If all lobbies are full
		playerLobby = new Lobby();
		playerLobby->players.push_back(newLobbyPlayer);
		playerLobby->timer = m_ActivityInfo.waitTime / 1000;
		m_Queue.push_back(playerLobby);
	}

	if (m_ActivityInfo.maxTeamSize != 1 && playerLobby->players.size() >= m_ActivityInfo.minTeamSize || m_ActivityInfo.maxTeamSize == 1 && playerLobby->players.size() >= m_ActivityInfo.minTeams) {
		// Update the joining player on the match timer
		std::string matchTimerUpdate = "time=3:" + std::to_string(playerLobby->timer);
		GameMessages::SendMatchUpdate(player, player->GetSystemAddress(), matchTimerUpdate, eMatchUpdate::PHASE_WAIT_READY);
	}
}

void ActivityComponent::PlayerLeave(LWOOBJID playerID) {

	// Removes the player from a lobby and notifies the others, not applicable for non-lobby instances
	for (Lobby* lobby : m_Queue) {
		for (int i = 0; i < lobby->players.size(); ++i) {
			if (lobby->players[i]->entityID == playerID) {
				std::string matchUpdateLeft = "player=9:" + std::to_string(playerID);
				for (LobbyPlayer* lobbyPlayer : lobby->players) {
					auto* entity = lobbyPlayer->GetEntity();
					if (entity == nullptr)
						continue;

					GameMessages::SendMatchUpdate(entity, entity->GetSystemAddress(), matchUpdateLeft, eMatchUpdate::PLAYER_REMOVED);
				}

				delete lobby->players[i];
				lobby->players[i] = nullptr;
				lobby->players.erase(lobby->players.begin() + i);

				return;
			}
		}
	}
}

void ActivityComponent::Update(float deltaTime) {
	std::vector<Lobby*> lobbiesToRemove{};
	// Ticks all the lobbies, not applicable for non-instance activities
	for (Lobby* lobby : m_Queue) {
		for (LobbyPlayer* player : lobby->players) {
			auto* entity = player->GetEntity();
			if (entity == nullptr) {
				PlayerLeave(player->entityID);
				return;
			}
		}

		if (lobby->players.empty()) {
			lobbiesToRemove.push_back(lobby);
			continue;
		}

		// Update the match time for all players
		if (m_ActivityInfo.maxTeamSize != 1 && lobby->players.size() >= m_ActivityInfo.minTeamSize
			|| m_ActivityInfo.maxTeamSize == 1 && lobby->players.size() >= m_ActivityInfo.minTeams) {
			if (lobby->timer == m_ActivityInfo.waitTime / 1000) {
				for (LobbyPlayer* joinedPlayer : lobby->players) {
					auto* entity = joinedPlayer->GetEntity();

					if (entity == nullptr)
						continue;

					std::string matchTimerUpdate = "time=3:" + std::to_string(lobby->timer);
					GameMessages::SendMatchUpdate(entity, entity->GetSystemAddress(), matchTimerUpdate, eMatchUpdate::PHASE_WAIT_READY);
				}
			}

			lobby->timer -= deltaTime;
		}

		bool lobbyReady = true;
		for (LobbyPlayer* player : lobby->players) {
			if (player->ready) continue;
			lobbyReady = false;
		}

		// If everyone's ready, jump the timer
		if (lobbyReady && lobby->timer > m_ActivityInfo.startDelay / 1000) {
			lobby->timer = m_ActivityInfo.startDelay / 1000;

			// Update players in lobby on switch to start delay
			std::string matchTimerUpdate = "time=3:" + std::to_string(lobby->timer);
			for (LobbyPlayer* player : lobby->players) {
				auto* entity = player->GetEntity();

				if (entity == nullptr)
					continue;

				GameMessages::SendMatchUpdate(entity, entity->GetSystemAddress(), matchTimerUpdate, eMatchUpdate::PHASE_WAIT_START);
			}
		}

		// The timer has elapsed, start the instance
		if (lobby->timer <= 0.0f) {
			LOG("Setting up instance.");
			ActivityInstance* instance = NewInstance();
			LoadPlayersIntoInstance(instance, lobby->players);
			instance->StartZone();
			lobbiesToRemove.push_back(lobby);
		}
	}

	while (!lobbiesToRemove.empty()) {
		RemoveLobby(lobbiesToRemove.front());
		lobbiesToRemove.erase(lobbiesToRemove.begin());
	}
}

void ActivityComponent::RemoveLobby(Lobby* lobby) {
	for (int i = 0; i < m_Queue.size(); ++i) {
		if (m_Queue[i] == lobby) {
			m_Queue.erase(m_Queue.begin() + i);
			return;
		}
	}
}

bool ActivityComponent::HasLobby() const {
	// If the player is not in the world he has to be, create a lobby for the transfer
	return m_ActivityInfo.instanceMapID != UINT_MAX && m_ActivityInfo.instanceMapID != Game::server->GetZoneID();
}

bool ActivityComponent::PlayerIsInQueue(Entity* player) {
	for (Lobby* lobby : m_Queue) {
		for (LobbyPlayer* lobbyPlayer : lobby->players) {
			if (player->GetObjectID() == lobbyPlayer->entityID) return true;
		}
	}

	return false;
}

bool ActivityComponent::IsPlayedBy(Entity* player) const {
	for (const auto* instance : this->m_Instances) {
		for (const auto* instancePlayer : instance->GetParticipants()) {
			if (instancePlayer != nullptr && instancePlayer->GetObjectID() == player->GetObjectID())
				return true;
		}
	}

	return false;
}

bool ActivityComponent::IsPlayedBy(LWOOBJID playerID) const {
	for (const auto* instance : this->m_Instances) {
		for (const auto* instancePlayer : instance->GetParticipants()) {
			if (instancePlayer != nullptr && instancePlayer->GetObjectID() == playerID)
				return true;
		}
	}

	return false;
}

bool ActivityComponent::TakeCost(Entity* player) const {
	if (m_ActivityInfo.optionalCostLOT <= 0 || m_ActivityInfo.optionalCostCount <= 0)
		return true;

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();
	if (inventoryComponent == nullptr)
		return false;

	if (inventoryComponent->GetLotCount(m_ActivityInfo.optionalCostLOT) < m_ActivityInfo.optionalCostCount)
		return false;

	inventoryComponent->RemoveItem(m_ActivityInfo.optionalCostLOT, m_ActivityInfo.optionalCostCount);

	return true;
}

void ActivityComponent::PlayerReady(Entity* player, bool bReady) {
	for (Lobby* lobby : m_Queue) {
		for (LobbyPlayer* lobbyPlayer : lobby->players) {
			if (lobbyPlayer->entityID == player->GetObjectID()) {

				lobbyPlayer->ready = bReady;

				// Update players in lobby on player being ready
				std::string matchReadyUpdate = "player=9:" + std::to_string(player->GetObjectID());
				eMatchUpdate readyStatus = eMatchUpdate::PLAYER_READY;
				if (!bReady) readyStatus = eMatchUpdate::PLAYER_NOT_READY;
				for (LobbyPlayer* otherPlayer : lobby->players) {
					auto* entity = otherPlayer->GetEntity();
					if (entity == nullptr)
						continue;

					GameMessages::SendMatchUpdate(entity, entity->GetSystemAddress(), matchReadyUpdate, readyStatus);
				}
			}
		}
	}
}

ActivityInstance* ActivityComponent::NewInstance() {
	auto* instance = new ActivityInstance(m_Parent, m_ActivityInfo);
	m_Instances.push_back(instance);
	return instance;
}

void ActivityComponent::LoadPlayersIntoInstance(ActivityInstance* instance, const std::vector<LobbyPlayer*>& lobby) const {
	for (LobbyPlayer* player : lobby) {
		auto* entity = player->GetEntity();
		if (entity == nullptr || !TakeCost(entity)) {
			continue;
		}

		instance->AddParticipant(entity);
	}
}

const std::vector<ActivityInstance*>& ActivityComponent::GetInstances() const {
	return m_Instances;
}

ActivityInstance* ActivityComponent::GetInstance(const LWOOBJID playerID) {
	for (const auto* instance : GetInstances()) {
		for (const auto* participant : instance->GetParticipants()) {
			if (participant->GetObjectID() == playerID)
				return const_cast<ActivityInstance*>(instance);
		}
	}

	return nullptr;
}

void ActivityComponent::ClearInstances() {
	for (ActivityInstance* instance : m_Instances) {
		delete instance;
	}
	m_Instances.clear();
}

ActivityPlayer* ActivityComponent::GetActivityPlayerData(LWOOBJID playerID) {
	for (auto* activityData : m_ActivityPlayers) {
		if (activityData->playerID == playerID) {
			return activityData;
		}
	}

	return nullptr;
}

void ActivityComponent::RemoveActivityPlayerData(LWOOBJID playerID) {
	for (size_t i = 0; i < m_ActivityPlayers.size(); i++) {
		if (m_ActivityPlayers[i]->playerID == playerID) {
			delete m_ActivityPlayers[i];
			m_ActivityPlayers[i] = nullptr;

			m_ActivityPlayers.erase(m_ActivityPlayers.begin() + i);
			m_DirtyActivityInfo = true;
			Game::entityManager->SerializeEntity(m_Parent);

			return;
		}
	}
}

ActivityPlayer* ActivityComponent::AddActivityPlayerData(LWOOBJID playerID) {
	auto* data = GetActivityPlayerData(playerID);
	if (data != nullptr)
		return data;

	m_ActivityPlayers.push_back(new ActivityPlayer{ playerID, {} });
	m_DirtyActivityInfo = true;
	Game::entityManager->SerializeEntity(m_Parent);

	return GetActivityPlayerData(playerID);
}

float_t ActivityComponent::GetActivityValue(LWOOBJID playerID, uint32_t index) {
	auto value = -1.0f;

	auto* data = GetActivityPlayerData(playerID);
	if (data != nullptr) {
		value = data->values[std::min(index, static_cast<uint32_t>(9))];
	}

	return value;
}

void ActivityComponent::SetActivityValue(LWOOBJID playerID, uint32_t index, float_t value) {
	auto* data = AddActivityPlayerData(playerID);
	if (data != nullptr) {
		data->values[std::min(index, static_cast<uint32_t>(9))] = value;
	}
	m_DirtyActivityInfo = true;
	Game::entityManager->SerializeEntity(m_Parent);
}

void ActivityComponent::PlayerRemove(LWOOBJID playerID) {
	for (auto* instance : GetInstances()) {
		auto participants = instance->GetParticipants();
		for (const auto* participant : participants) {
			if (participant != nullptr && participant->GetObjectID() == playerID) {
				instance->RemoveParticipant(participant);
				RemoveActivityPlayerData(playerID);

				// If the instance is empty after the delete of the participant, delete the instance too
				if (instance->GetParticipants().empty()) {
					m_Instances.erase(std::find(m_Instances.begin(), m_Instances.end(), instance));
					delete instance;
				}
				return;
			}
		}
	}
}

void ActivityInstance::StartZone() {
	if (m_Participants.empty())
		return;

	const auto& participants = GetParticipants();
	if (participants.empty())
		return;

	auto* leader = participants[0];
	LWOZONEID zoneId = LWOZONEID(m_ActivityInfo.instanceMapID, 0, leader->GetCharacter()->GetPropertyCloneID());

	// only make a team if we have more than one participant
	if (participants.size() > 1) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::CREATE_TEAM);

		bitStream.Write(leader->GetObjectID());
		bitStream.Write(m_Participants.size());

		for (const auto& participant : m_Participants) {
			bitStream.Write(participant);
		}

		bitStream.Write(zoneId);

		Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
	}

	const auto cloneId = GeneralUtils::GenerateRandomNumber<uint32_t>(1, UINT32_MAX);
	for (Entity* player : participants) {
		const auto objid = player->GetObjectID();
		ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, m_ActivityInfo.instanceMapID, cloneId, false, [objid](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {

			auto* player = Game::entityManager->GetEntity(objid);
			if (player == nullptr)
				return;

			LOG("Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", player->GetCharacter()->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);
			if (player->GetCharacter()) {
				player->GetCharacter()->SetZoneID(zoneID);
				player->GetCharacter()->SetZoneInstance(zoneInstance);
				player->GetCharacter()->SetZoneClone(zoneClone);
			}

			WorldPackets::SendTransferToWorld(player->GetSystemAddress(), serverIP, serverPort, mythranShift);
			return;
			});
	}

	m_NextZoneCloneID++;
}

void ActivityInstance::RewardParticipant(Entity* participant) {
	auto* missionComponent = participant->GetComponent<MissionComponent>();
	if (missionComponent) {
		missionComponent->Progress(eMissionTaskType::ACTIVITY, m_ActivityInfo.ActivityID);
	}

	// First, get the activity data
	auto* activityRewardsTable = CDClientManager::Instance().GetTable<CDActivityRewardsTable>();
	std::vector<CDActivityRewards> activityRewards = activityRewardsTable->Query([this](CDActivityRewards entry) { return (entry.objectTemplate == m_ActivityInfo.ActivityID); });

	if (!activityRewards.empty()) {
		uint32_t minCoins = 0;
		uint32_t maxCoins = 0;

		auto* currencyTableTable = CDClientManager::Instance().GetTable<CDCurrencyTableTable>();
		std::vector<CDCurrencyTable> currencyTable = currencyTableTable->Query([=](CDCurrencyTable entry) { return (entry.currencyIndex == activityRewards[0].CurrencyIndex && entry.npcminlevel == 1); });

		if (!currencyTable.empty()) {
			minCoins = currencyTable[0].minvalue;
			maxCoins = currencyTable[0].maxvalue;
		}

		Loot::DropLoot(participant, m_Parent, activityRewards[0].LootMatrixIndex, minCoins, maxCoins);
	}
}

std::vector<Entity*> ActivityInstance::GetParticipants() const {
	std::vector<Entity*> entities;
	entities.reserve(m_Participants.size());

	for (const auto& id : m_Participants) {
		auto* entity = Game::entityManager->GetEntity(id);
		if (entity != nullptr)
			entities.push_back(entity);
	}

	return entities;
}

void ActivityInstance::AddParticipant(Entity* participant) {
	const auto id = participant->GetObjectID();
	if (std::count(m_Participants.begin(), m_Participants.end(), id))
		return;

	m_Participants.push_back(id);
}

void ActivityInstance::RemoveParticipant(const Entity* participant) {
	const auto loadedParticipant = std::find(m_Participants.begin(), m_Participants.end(), participant->GetObjectID());
	if (loadedParticipant != m_Participants.end()) {
		m_Participants.erase(loadedParticipant);
	}
}

uint32_t ActivityInstance::GetScore() const {
	return score;
}

void ActivityInstance::SetScore(uint32_t score) {
	this->score = score;
}

Entity* LobbyPlayer::GetEntity() const {
	return Game::entityManager->GetEntity(entityID);
}
