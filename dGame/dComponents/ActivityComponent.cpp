#include "ActivityComponent.h"
#include "ChatServerLink.h"
#include "ChatPackets.h"
#include "GameMessages.h"
#include "ActivityMessages.h"
#include "CDClientManager.h"
#include "MissionComponent.h"
#include "Character.h"
#include "dZoneManager.h"
#include "Game.h"
#include "Logger.h"
#include "ClientPackets.h"
#include "EntityManager.h"
#include "ChatPackets.h"
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
#include "ServiceType.h"
#include "MessageType/Chat.h"

#include "CDActivityRewardsTable.h"
#include "CDActivitiesTable.h"
#include "LeaderboardManager.h"
#include "CharacterComponent.h"
#include "Amf3.h"
#include <ranges>

namespace {
	const ActivityInstance g_EmptyInstance{ nullptr, CDActivities{} };
}

ActivityComponent::ActivityComponent(Entity* parent, int32_t componentID) : Component(parent, componentID) {
	RegisterMsg(&ActivityComponent::OnGetObjectReportInfo);
	/*
	* This is precisely what the client does functionally
	* Use the component id as the default activity id and load its data from the database
	* if activityID is specified and if that column exists in the activities table, update the activity info with that data.
	*/

	m_ActivityID = componentID;
	LoadActivityData(componentID);
	if (m_Parent->HasVar(u"activityID")) {
		m_ActivityID = parent->GetVar<int32_t>(u"activityID");
		LoadActivityData(m_ActivityID);
	}
}
void ActivityComponent::LoadActivityData(const int32_t activityId) {
	CDActivitiesTable* activitiesTable = CDClientManager::GetTable<CDActivitiesTable>();
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

void ActivityComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(m_DirtyActivityInfo);
	if (m_DirtyActivityInfo) {
		outBitStream.Write<uint32_t>(m_ActivityPlayers.size());
		if (!m_ActivityPlayers.empty()) {
			for (const auto& [playerID, values] : m_ActivityPlayers) {
				outBitStream.Write<LWOOBJID>(playerID);
				for (const auto& activityValue : values) {
					outBitStream.Write<float_t>(activityValue);
				}
			}
		}
		if (!bIsInitialUpdate) m_DirtyActivityInfo = false;
	}
}

void ActivityComponent::ReloadConfig() {
	CDActivitiesTable* activitiesTable = CDClientManager::GetTable<CDActivitiesTable>();
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

void ActivityComponent::PlayerJoin(Entity* player, const std::string& playerChoices) {
	// If we have a lobby, queue the player and allow others to join, otherwise spin up an instance on the spot
	if (HasLobby()) {
		PlayerJoinLobby(player, playerChoices);
	} else if (!IsPlayedBy(player)) {
		NewInstance().AddParticipant(player);
	}
}

namespace {
	void SendMatchRequest(const LWOOBJID playerID, const ChatPackets::eMatchRequestType type, const int32_t value) {
		ChatPackets::MatchRequest request;
		request.playerID = playerID;
		request.type = type;
		request.value = value;
		ChatServerLink::Send(request);
	}
}

void ActivityComponent::PlayerJoinLobby(Entity* player, const std::string& playerChoices) {
	if (!m_Parent->HasComponent(eReplicaComponentType::QUICK_BUILD)) {
		GameMessages::MatchResponse matchResponse;
		matchResponse.target = player->GetObjectID();
		matchResponse.response = 0;
		matchResponse.SendToClient(player->GetSystemAddress()); // tell the client they joined a lobby
	}

	auto* character = player->GetCharacter();
	if (character != nullptr)
		character->SetLastNonInstanceZoneID(Game::zoneManager->GetZone()->GetWorldID());

	// The lobbies are the chat server's, so players of every instance of this zone wait together (docs/Matchmaking.md).
	// It gets what this world read from the activity, overrides (solo racing, transfer zone) included.
	ChatPackets::MatchRequest request;
	request.playerID = player->GetObjectID();
	request.type = ChatPackets::eMatchRequestType::JOIN;
	request.activityID = m_ActivityInfo.ActivityID;
	request.playerName = character != nullptr ? character->GetName() : "";
	request.playerChoices = playerChoices;
	request.instanceMapID = m_ActivityInfo.instanceMapID;
	request.minTeams = m_ActivityInfo.minTeams;
	request.maxTeams = m_ActivityInfo.maxTeams;
	request.minTeamSize = m_ActivityInfo.minTeamSize;
	request.maxTeamSize = m_ActivityInfo.maxTeamSize;
	request.waitTime = m_ActivityInfo.waitTime;
	request.startDelay = m_ActivityInfo.startDelay;
	ChatServerLink::Send(request);
}

void ActivityComponent::PlayerLeave(LWOOBJID playerID) {
	// Not applicable for non-lobby instances
	if (!HasLobby()) return;
	SendMatchRequest(playerID, ChatPackets::eMatchRequestType::LEAVE, 0);
}

void ActivityComponent::StartMatch(const ChatPackets::MatchTransfer& transfer) {
	auto& instance = NewInstance();
	for (const auto playerID : transfer.players) {
		auto* const entity = Game::entityManager->GetEntity(playerID);
		if (entity == nullptr || !CheckCost(entity)) {
			continue;
		}

		instance.AddParticipant(entity);
	}
	instance.TransferParticipants(transfer);
}

bool ActivityComponent::HasLobby() const {
	// If the player is not in the world he has to be, create a lobby for the transfer
	return m_ActivityInfo.instanceMapID != UINT_MAX && m_ActivityInfo.instanceMapID != Game::server->GetZoneID();
}

bool ActivityComponent::IsPlayedBy(Entity* player) const {
	for (const auto& instance : m_Instances) {
		for (const auto* instancePlayer : instance.GetParticipants()) {
			if (instancePlayer != nullptr && instancePlayer->GetObjectID() == player->GetObjectID())
				return true;
		}
	}

	return false;
}

bool ActivityComponent::IsPlayedBy(LWOOBJID playerID) const {
	for (const auto& instance : m_Instances) {
		for (const auto* instancePlayer : instance.GetParticipants()) {
			if (instancePlayer != nullptr && instancePlayer->GetObjectID() == playerID)
				return true;
		}
	}

	return false;
}

bool ActivityComponent::CheckCost(Entity* player) const {
	if (m_ActivityInfo.optionalCostLOT <= 0 || m_ActivityInfo.optionalCostCount <= 0)
		return true;

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();
	if (inventoryComponent == nullptr)
		return false;

	if (inventoryComponent->GetLotCount(m_ActivityInfo.optionalCostLOT) < m_ActivityInfo.optionalCostCount)
		return false;

	return true;
}

bool ActivityComponent::TakeCost(Entity* player) const {
	auto* inventoryComponent = player->GetComponent<InventoryComponent>();
	return CheckCost(player) && inventoryComponent && inventoryComponent->RemoveItem(m_ActivityInfo.optionalCostLOT, m_ActivityInfo.optionalCostCount, eInventoryType::ALL);
}

void ActivityComponent::PlayerReady(Entity* player, bool bReady) {
	SendMatchRequest(player->GetObjectID(), ChatPackets::eMatchRequestType::READY, bReady ? 1 : 0);
}

ActivityInstance& ActivityComponent::NewInstance() {
	m_Instances.push_back(ActivityInstance(m_Parent, m_ActivityInfo));
	return m_Instances.back();
}

const ActivityInstance& ActivityComponent::GetInstance(const LWOOBJID playerID) const {
	for (const auto& instance : m_Instances) {
		for (const auto* participant : instance.GetParticipants()) {
			if (participant->GetObjectID() == playerID)
				return instance;
		}
	}

	return g_EmptyInstance;
}

bool ActivityComponent::PlayerHasActivityData(LWOOBJID playerID) const {
	return m_ActivityPlayers.contains(playerID);
}

void ActivityComponent::RemoveActivityPlayerData(LWOOBJID playerID) {
	m_ActivityPlayers.erase(playerID);
	m_DirtyActivityInfo = true;
}

float_t ActivityComponent::GetActivityValue(LWOOBJID playerID, uint32_t index) const {
	float value = -1.0f;

	const auto& data = m_ActivityPlayers.find(playerID);
	if (data != m_ActivityPlayers.cend()) {
		value = data->second[std::min(index, static_cast<uint32_t>(9))];
	}
	LOG_DEBUG("Player %llu has score %f at index %i", playerID, value, index);
	return value;
}

void ActivityComponent::SetActivityValue(LWOOBJID playerID, uint32_t index, float_t value) {
	auto& data = m_ActivityPlayers[playerID];
	data[std::min(index, static_cast<uint32_t>(9))] = value;
	LOG_DEBUG("%llu index %i has score of %f", playerID, index, value);
	m_DirtyActivityInfo = true;
	Game::entityManager->SerializeEntity(m_Parent);
}

void ActivityComponent::PlayerRemove(LWOOBJID playerID) {
	for (int i = 0; i < m_Instances.size(); i++) {
		auto& instance = m_Instances[i];
		auto participants = instance.GetParticipants();
		for (const auto* participant : participants) {
			if (participant != nullptr && participant->GetObjectID() == playerID) {
				instance.RemoveParticipant(participant);
				RemoveActivityPlayerData(playerID);

				// If the instance is empty after the delete of the participant, delete the instance too
				if (instance.GetParticipants().empty()) {
					m_Instances.erase(m_Instances.begin() + i);
				}
				return;
			}
		}
	}
}

void ActivityInstance::TransferParticipants(const ChatPackets::MatchTransfer& transfer) {
	const auto zoneID = transfer.zoneID.GetMapID();
	const auto zoneInstance = transfer.zoneID.GetInstanceID();
	const auto zoneClone = transfer.zoneID.GetCloneID();
	for (Entity* player : GetParticipants()) {
		LOG("Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", player->GetCharacter()->GetName().c_str(), zoneID, zoneInstance, zoneClone, transfer.mythranShift == true ? "true" : "false", transfer.serverIP.c_str(), transfer.serverPort);
		if (player->GetCharacter()) {
			auto* characterComponent = player->GetComponent<CharacterComponent>();
			if (characterComponent) {
				characterComponent->AddVisitedLevel(LWOZONEID(zoneID, LWOINSTANCEID_INVALID, zoneClone));
			}

			player->GetCharacter()->SetZoneID(zoneID);
			player->GetCharacter()->SetZoneInstance(zoneInstance);
			player->GetCharacter()->SetZoneClone(zoneClone);
		}

		ClientPackets::TransferToWorld transferToWorld;
		transferToWorld.serverIP = LUString(transfer.serverIP);
		transferToWorld.serverPort = transfer.serverPort;
		transferToWorld.mythranShift = transfer.mythranShift;
		transferToWorld.Send(player->GetSystemAddress());
	}

	m_NextZoneCloneID++;
}

void ActivityInstance::RewardParticipant(Entity* participant) {
	auto* missionComponent = participant->GetComponent<MissionComponent>();
	if (missionComponent) {
		missionComponent->Progress(eMissionTaskType::ACTIVITY, m_ActivityInfo.ActivityID);
	}

	// First, get the activity data
	auto* activityRewardsTable = CDClientManager::GetTable<CDActivityRewardsTable>();
	std::vector<CDActivityRewards> activityRewards = activityRewardsTable->Query([this](CDActivityRewards entry) { return (entry.objectTemplate == m_ActivityInfo.ActivityID); });

	if (!activityRewards.empty()) {
		const auto [minCoins, maxCoins] = Loot::GetActivityCoinRange(activityRewards[0]);

		Loot::DropLoot(participant, m_Parent->GetObjectID(), activityRewards[0].LootMatrixIndex, minCoins, maxCoins);
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

bool ActivityComponent::OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo) {
	auto& activityInfo = reportInfo.info->PushDebug("Activity");

	auto& instances = activityInfo.PushDebug("Instances: " + std::to_string(m_Instances.size()));
	size_t i = 0;
	for (const auto& activityInstance : m_Instances) {
		auto& instance = instances.PushDebug("Instance " + std::to_string(i++));
		instance.PushDebug<AMFIntValue>("Score") = activityInstance.GetScore();
		instance.PushDebug<AMFIntValue>("Next Zone Clone ID") = activityInstance.GetNextZoneCloneID();

		{
			auto& activityInfo = instance.PushDebug("Activity Info");
			const auto& instanceActInfo = activityInstance.GetActivityInfo();
			activityInfo.PushDebug<AMFIntValue>("ActivityID") = instanceActInfo.ActivityID;
			activityInfo.PushDebug<AMFIntValue>("locStatus") = instanceActInfo.locStatus;
			activityInfo.PushDebug<AMFIntValue>("instanceMapID") = instanceActInfo.instanceMapID;
			activityInfo.PushDebug<AMFIntValue>("minTeams") = instanceActInfo.minTeams;
			activityInfo.PushDebug<AMFIntValue>("maxTeams") = instanceActInfo.maxTeams;
			activityInfo.PushDebug<AMFIntValue>("minTeamSize") = instanceActInfo.minTeamSize;
			activityInfo.PushDebug<AMFIntValue>("maxTeamSize") = instanceActInfo.maxTeamSize;
			activityInfo.PushDebug<AMFIntValue>("waitTime") = instanceActInfo.waitTime;
			activityInfo.PushDebug<AMFIntValue>("startDelay") = instanceActInfo.startDelay;
			activityInfo.PushDebug<AMFBoolValue>("requiresUniqueData") = instanceActInfo.requiresUniqueData;
			activityInfo.PushDebug<AMFIntValue>("leaderboardType") = instanceActInfo.leaderboardType;
			activityInfo.PushDebug<AMFBoolValue>("localize") = instanceActInfo.localize;
			activityInfo.PushDebug<AMFIntValue>("optionalCostLOT") = instanceActInfo.optionalCostLOT;
			activityInfo.PushDebug<AMFIntValue>("optionalCostCount") = instanceActInfo.optionalCostCount;
			activityInfo.PushDebug<AMFBoolValue>("showUIRewards") = instanceActInfo.showUIRewards;
			activityInfo.PushDebug<AMFIntValue>("CommunityActivityFlagID") = instanceActInfo.CommunityActivityFlagID;
			activityInfo.PushDebug<AMFStringValue>("gate_version") = instanceActInfo.gate_version;
			activityInfo.PushDebug<AMFBoolValue>("noTeamLootOnDeath") = instanceActInfo.noTeamLootOnDeath;
			activityInfo.PushDebug<AMFDoubleValue>("optionalPercentage") = instanceActInfo.optionalPercentage;
		}

		auto& participants = instance.PushDebug("Participants");
		for (const auto* participant : activityInstance.GetParticipants()) {
			if (!participant) continue;
			auto* character = participant->GetCharacter();
			if (!character) continue;
			participants.PushDebug<AMFStringValue>(std::to_string(participant->GetObjectID()) + ": " + character->GetName()) = "";
		}
	}

	auto& activityPlayers = activityInfo.PushDebug("Activity Players");
	for (const auto& [playerID, playerScores] : m_ActivityPlayers) {
		auto* const activityPlayerEntity = Game::entityManager->GetEntity(playerID);
		if (!activityPlayerEntity) continue;
		auto* character = activityPlayerEntity->GetCharacter();
		if (!character) continue;

		auto& playerData = activityPlayers.PushDebug(std::to_string(playerID) + " " + character->GetName());

		auto& scores = playerData.PushDebug("Scores");
		for (size_t i = 0; i < 10; ++i) {
			scores.PushDebug<AMFDoubleValue>(std::to_string(i)) = playerScores[i];
		}
	}

	activityInfo.PushDebug<AMFIntValue>("ActivityID") = m_ActivityID;
	return true;
}
