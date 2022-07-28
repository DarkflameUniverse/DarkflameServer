#include "ScriptedActivityComponent.h"
#include "GameMessages.h"
#include "CDClientManager.h"
#include "MissionComponent.h"
#include "Character.h"
#include "dZoneManager.h"
#include "ZoneInstanceManager.h"
#include "Game.h"
#include "dLogger.h"
#include <WorldPackets.h>
#include "EntityManager.h"
#include "ChatPackets.h"
#include "Player.h"
#include "PacketUtils.h"
#include "dServer.h"
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "dConfig.h"
#include "DestroyableComponent.h"

ScriptedActivityComponent::ScriptedActivityComponent(Entity* parent, int activityID) : Component(parent) {
	CDActivitiesTable* activitiesTable = CDClientManager::Instance()->GetTable<CDActivitiesTable>("Activities");
	std::vector<CDActivities> activities = activitiesTable->Query([=](CDActivities entry) {return (entry.ActivityID == activityID); });

	for (CDActivities activity : activities) {
		m_ActivityInfo = activity;

		const auto mapID = m_ActivityInfo.instanceMapID;

		if ((mapID == 1203 || mapID == 1261 || mapID == 1303 || mapID == 1403) && Game::config->GetValue("solo_racing") == "1") {
			m_ActivityInfo.minTeamSize = 1;
			m_ActivityInfo.minTeams = 1;
		}

		const auto& transferOverride = parent->GetVar<std::u16string>(u"transferZoneID");
		if (!transferOverride.empty()) {
			m_ActivityInfo.instanceMapID = std::stoi(GeneralUtils::UTF16ToWTF8(transferOverride));

			// TODO: LU devs made me do it (for some reason cannon cove instancer is marked to go to GF survival)
			// NOTE: 1301 is GF survival
			if (m_ActivityInfo.instanceMapID == 1301) {
				m_ActivityInfo.instanceMapID = 1302;
			}
		}
	}

	auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();

	if (destroyableComponent) {
		// check for LMIs and set the loot LMIs
		CDActivityRewardsTable* activityRewardsTable = CDClientManager::Instance()->GetTable<CDActivityRewardsTable>("ActivityRewards");
		std::vector<CDActivityRewards> activityRewards = activityRewardsTable->Query([=](CDActivityRewards entry) {return (entry.LootMatrixIndex == destroyableComponent->GetLootMatrixID()); });

		uint32_t startingLMI = 0;

		if (activityRewards.size() > 0) {
			startingLMI = activityRewards[0].LootMatrixIndex;
		}

		if (startingLMI > 0) {
			// now time for bodge :)

			std::vector<CDActivityRewards> objectTemplateActivities = activityRewardsTable->Query([=](CDActivityRewards entry) {return (activityRewards[0].objectTemplate == entry.objectTemplate); });
			for (const auto& item : objectTemplateActivities) {
				if (item.activityRating > 0 && item.activityRating < 5) {
					m_ActivityLootMatrices.insert({ item.activityRating, item.LootMatrixIndex });
				}
			}
		}
	}
}

ScriptedActivityComponent::~ScriptedActivityComponent()
= default;

void ScriptedActivityComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) const {
	outBitStream->Write(true);
	outBitStream->Write<uint32_t>(m_ActivityPlayers.size());

	if (!m_ActivityPlayers.empty()) {
		for (const auto& activityPlayer : m_ActivityPlayers) {

			outBitStream->Write<LWOOBJID>(activityPlayer->playerID);
			for (const auto& activityValue : activityPlayer->values) {
				outBitStream->Write<float_t>(activityValue);
			}
		}
	}
}

void ScriptedActivityComponent::HandleMessageBoxResponse(Entity* player, const std::string& id) {
	if (m_ActivityInfo.ActivityID == 103) {
		return;
	}

	if (id == "LobbyExit") {
		PlayerLeave(player->GetObjectID());
	} else if (id == "PlayButton") {
		PlayerJoin(player);
	}
}

void ScriptedActivityComponent::PlayerJoin(Entity* player) {
	if (m_ActivityInfo.ActivityID == 103 || PlayerIsInQueue(player) || !IsValidActivity(player)) {
		return;
	}

	// If we have a lobby, queue the player and allow others to join, otherwise spin up an instance on the spot
	if (HasLobby()) {
		PlayerJoinLobby(player);
	} else if (!IsPlayedBy(player)) {
		auto* instance = NewInstance();
		instance->AddParticipant(player);
	}

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void ScriptedActivityComponent::PlayerJoinLobby(Entity* player) {
	if (!m_Parent->HasComponent(COMPONENT_TYPE_REBUILD))
		GameMessages::SendMatchResponse(player, player->GetSystemAddress(), 0); // tell the client they joined a lobby
	LobbyPlayer* newLobbyPlayer = new LobbyPlayer();
	newLobbyPlayer->entityID = player->GetObjectID();
	Lobby* playerLobby = nullptr;

	auto* character = player->GetCharacter();
	if (character != nullptr)
		character->SetLastNonInstanceZoneID(dZoneManager::Instance()->GetZone()->GetWorldID());

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
				GameMessages::SendMatchUpdate(player, player->GetSystemAddress(), matchUpdate, eMatchUpdate::MATCH_UPDATE_PLAYER_JOINED);
				PlayerReady(entity, joinedPlayer->ready);
				GameMessages::SendMatchUpdate(entity, entity->GetSystemAddress(), matchUpdateJoined, eMatchUpdate::MATCH_UPDATE_PLAYER_JOINED);
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
		GameMessages::SendMatchUpdate(player, player->GetSystemAddress(), matchTimerUpdate, eMatchUpdate::MATCH_UPDATE_TIME);
	}
}

void ScriptedActivityComponent::PlayerLeave(LWOOBJID playerID) {

	// Removes the player from a lobby and notifies the others, not applicable for non-lobby instances
	for (Lobby* lobby : m_Queue) {
		for (int i = 0; i < lobby->players.size(); ++i) {
			if (lobby->players[i]->entityID == playerID) {
				std::string matchUpdateLeft = "player=9:" + std::to_string(playerID);
				for (LobbyPlayer* lobbyPlayer : lobby->players) {
					auto* entity = lobbyPlayer->GetEntity();
					if (entity == nullptr)
						continue;

					GameMessages::SendMatchUpdate(entity, entity->GetSystemAddress(), matchUpdateLeft, eMatchUpdate::MATCH_UPDATE_PLAYER_LEFT);
				}

				delete lobby->players[i];
				lobby->players[i] = nullptr;
				lobby->players.erase(lobby->players.begin() + i);

				return;
			}
		}
	}
}

void ScriptedActivityComponent::Update(float deltaTime) {

	// Ticks all the lobbies, not applicable for non-instance activities
	for (Lobby* lobby : m_Queue) {
		for (LobbyPlayer* player : lobby->players) {
			auto* entity = player->GetEntity();
			if (entity == nullptr) {
				PlayerLeave(player->entityID);
				return;
			}
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
					GameMessages::SendMatchUpdate(entity, entity->GetSystemAddress(), matchTimerUpdate, eMatchUpdate::MATCH_UPDATE_TIME);
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

				GameMessages::SendMatchUpdate(entity, entity->GetSystemAddress(), matchTimerUpdate, eMatchUpdate::MATCH_UPDATE_TIME_START_DELAY);
			}
		}

		// The timer has elapsed, start the instance
		if (lobby->timer <= 0.0f) {
			Game::logger->Log("ScriptedActivityComponent", "Setting up instance.");

			ActivityInstance* instance = NewInstance();
			LoadPlayersIntoInstance(instance, lobby->players);
			RemoveLobby(lobby);
			instance->StartZone();
		}
	}
}

void ScriptedActivityComponent::RemoveLobby(Lobby* lobby) {
	for (int i = 0; i < m_Queue.size(); ++i) {
		if (m_Queue[i] == lobby) {
			m_Queue.erase(m_Queue.begin() + i);
			return;
		}
	}
}

bool ScriptedActivityComponent::HasLobby() const {
	// If the player is not in the world he has to be, create a lobby for the transfer
	return m_ActivityInfo.instanceMapID != UINT_MAX && m_ActivityInfo.instanceMapID != Game::server->GetZoneID();
}

bool ScriptedActivityComponent::IsValidActivity(Entity* player) {
	// Makes it so that scripted activities with an unimplemented map cannot be joined
	/*if (player->GetGMLevel() < GAME_MASTER_LEVEL_DEVELOPER && (m_ActivityInfo.instanceMapID == 1302 || m_ActivityInfo.instanceMapID == 1301)) {
		if (m_Parent->GetLOT() == 4860) {
			auto* missionComponent = player->GetComponent<MissionComponent>();
			missionComponent->CompleteMission(229);
		}

		ChatPackets::SendSystemMessage(player->GetSystemAddress(), u"Sorry, this activity is not ready.");
		static_cast<Player*>(player)->SendToZone(dZoneManager::Instance()->GetZone()->GetWorldID()); // Gets them out of this stuck state

		return false;
	}*/

	return true;
}

bool ScriptedActivityComponent::PlayerIsInQueue(Entity* player) {
	for (Lobby* lobby : m_Queue) {
		for (LobbyPlayer* lobbyPlayer : lobby->players) {
			if (player->GetObjectID() == lobbyPlayer->entityID) return true;
		}
	}

	return false;
}

bool ScriptedActivityComponent::IsPlayedBy(Entity* player) const {
	for (const auto* instance : this->m_Instances) {
		for (const auto* instancePlayer : instance->GetParticipants()) {
			if (instancePlayer != nullptr && instancePlayer->GetObjectID() == player->GetObjectID())
				return true;
		}
	}

	return false;
}

bool ScriptedActivityComponent::IsPlayedBy(LWOOBJID playerID) const {
	for (const auto* instance : this->m_Instances) {
		for (const auto* instancePlayer : instance->GetParticipants()) {
			if (instancePlayer != nullptr && instancePlayer->GetObjectID() == playerID)
				return true;
		}
	}

	return false;
}

bool ScriptedActivityComponent::TakeCost(Entity* player) const {
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

void ScriptedActivityComponent::PlayerReady(Entity* player, bool bReady) {
	for (Lobby* lobby : m_Queue) {
		for (LobbyPlayer* lobbyPlayer : lobby->players) {
			if (lobbyPlayer->entityID == player->GetObjectID()) {

				lobbyPlayer->ready = bReady;

				// Update players in lobby on player being ready
				std::string matchReadyUpdate = "player=9:" + std::to_string(player->GetObjectID());
				eMatchUpdate readyStatus = eMatchUpdate::MATCH_UPDATE_PLAYER_READY;
				if (!bReady) readyStatus = eMatchUpdate::MATCH_UPDATE_PLAYER_UNREADY;
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

ActivityInstance* ScriptedActivityComponent::NewInstance() {
	auto* instance = new ActivityInstance(m_Parent, m_ActivityInfo);
	m_Instances.push_back(instance);
	return instance;
}

void ScriptedActivityComponent::LoadPlayersIntoInstance(ActivityInstance* instance, const std::vector<LobbyPlayer*>& lobby) const {
	for (LobbyPlayer* player : lobby) {
		auto* entity = player->GetEntity();
		if (entity == nullptr || !TakeCost(entity)) {
			continue;
		}

		instance->AddParticipant(entity);
	}
}

const std::vector<ActivityInstance*>& ScriptedActivityComponent::GetInstances() const {
	return m_Instances;
}

ActivityInstance* ScriptedActivityComponent::GetInstance(const LWOOBJID playerID) {
	for (const auto* instance : GetInstances()) {
		for (const auto* participant : instance->GetParticipants()) {
			if (participant->GetObjectID() == playerID)
				return const_cast<ActivityInstance*>(instance);
		}
	}

	return nullptr;
}

void ScriptedActivityComponent::ClearInstances() {
	for (ActivityInstance* instance : m_Instances) {
		delete instance;
	}
	m_Instances.clear();
}

ActivityPlayer* ScriptedActivityComponent::GetActivityPlayerData(LWOOBJID playerID) {
	for (auto* activityData : m_ActivityPlayers) {
		if (activityData->playerID == playerID) {
			return activityData;
		}
	}

	return nullptr;
}

void ScriptedActivityComponent::RemoveActivityPlayerData(LWOOBJID playerID) {
	for (size_t i = 0; i < m_ActivityPlayers.size(); i++) {
		if (m_ActivityPlayers[i]->playerID == playerID) {
			delete m_ActivityPlayers[i];
			m_ActivityPlayers[i] = nullptr;

			m_ActivityPlayers.erase(m_ActivityPlayers.begin() + i);
			EntityManager::Instance()->SerializeEntity(m_Parent);

			return;
		}
	}
}

ActivityPlayer* ScriptedActivityComponent::AddActivityPlayerData(LWOOBJID playerID) {
	auto* data = GetActivityPlayerData(playerID);
	if (data != nullptr)
		return data;

	m_ActivityPlayers.push_back(new ActivityPlayer{ playerID, {} });
	EntityManager::Instance()->SerializeEntity(m_Parent);

	return GetActivityPlayerData(playerID);
}

float_t ScriptedActivityComponent::GetActivityValue(LWOOBJID playerID, uint32_t index) {
	auto value = -1.0f;

	auto* data = GetActivityPlayerData(playerID);
	if (data != nullptr) {
		value = data->values[std::min(index, (uint32_t)9)];
	}

	return value;
}

void ScriptedActivityComponent::SetActivityValue(LWOOBJID playerID, uint32_t index, float_t value) {
	auto* data = AddActivityPlayerData(playerID);
	if (data != nullptr) {
		data->values[std::min(index, (uint32_t)9)] = value;
	}

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void ScriptedActivityComponent::PlayerRemove(LWOOBJID playerID) {
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
		PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_CREATE_TEAM);

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

			auto* player = EntityManager::Instance()->GetEntity(objid);
			if (player == nullptr)
				return;

			Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", player->GetCharacter()->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);
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
		missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_ACTIVITY, m_ActivityInfo.ActivityID);
	}

	// First, get the activity data
	auto* activityRewardsTable = CDClientManager::Instance()->GetTable<CDActivityRewardsTable>("ActivityRewards");
	std::vector<CDActivityRewards> activityRewards = activityRewardsTable->Query([=](CDActivityRewards entry) { return (entry.objectTemplate == m_ActivityInfo.ActivityID); });

	if (!activityRewards.empty()) {
		uint32_t minCoins = 0;
		uint32_t maxCoins = 0;

		auto* currencyTableTable = CDClientManager::Instance()->GetTable<CDCurrencyTableTable>("CurrencyTable");
		std::vector<CDCurrencyTable> currencyTable = currencyTableTable->Query([=](CDCurrencyTable entry) { return (entry.currencyIndex == activityRewards[0].CurrencyIndex && entry.npcminlevel == 1); });

		if (!currencyTable.empty()) {
			minCoins = currencyTable[0].minvalue;
			maxCoins = currencyTable[0].maxvalue;
		}

		LootGenerator::Instance().DropLoot(participant, m_Parent, activityRewards[0].LootMatrixIndex, minCoins, maxCoins);
	}
}

std::vector<Entity*> ActivityInstance::GetParticipants() const {
	std::vector<Entity*> entities;
	entities.reserve(m_Participants.size());

	for (const auto& id : m_Participants) {
		auto* entity = EntityManager::Instance()->GetEntity(id);
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
	return EntityManager::Instance()->GetEntity(entityID);
}
