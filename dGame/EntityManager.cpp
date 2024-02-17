#include "EntityManager.h"
#include "RakNetTypes.h"
#include "Game.h"
#include "User.h"
#include "ObjectIDManager.h"
#include "Character.h"
#include "GeneralUtils.h"
#include "dServer.h"
#include "Spawner.h"
#include "SkillComponent.h"
#include "SwitchComponent.h"
#include "UserManager.h"
#include "Metrics.hpp"
#include "dZoneManager.h"
#include "MissionComponent.h"
#include "Game.h"
#include "Logger.h"
#include "MessageIdentifiers.h"
#include "dConfig.h"
#include "eTriggerEventType.h"
#include "eObjectBits.h"
#include "eGameMasterLevel.h"
#include "eReplicaComponentType.h"
#include "eReplicaPacketType.h"
#include "PlayerManager.h"
#include "GhostComponent.h"
#include "ServerPreconditions.h"
#include <ranges>

// Configure which zones have ghosting disabled, mostly small worlds.
std::vector<LWOMAPID> EntityManager::m_GhostingExcludedZones = {
	// Small zones
	1000,

	// Racing zones
	1203,
	1261,
	1303,
	1403,

	// Property zones
	1150,
	1151,
	1250,
	1251,
	1350,
	1450
};

// Configure some exceptions for ghosting, nessesary for some special objects.
std::vector<LOT> EntityManager::m_GhostingExcludedLOTs = {
	// AG - Footrace
	4967
};

void EntityManager::Initialize() {
	// Check if this zone has ghosting enabled
	m_GhostingEnabled = std::find(
		m_GhostingExcludedZones.begin(),
		m_GhostingExcludedZones.end(),
		Game::zoneManager->GetZoneID().GetMapID()
	) == m_GhostingExcludedZones.end();

	// grab hardcore mode settings and load them with sane defaults
	auto hcmode = Game::config->GetValue("hardcore_mode");
	m_HardcoreMode = hcmode.empty() ? false : (hcmode == "1");
	auto hcUscorePercent = Game::config->GetValue("hardcore_lose_uscore_on_death_percent");
	m_HardcoreLoseUscoreOnDeathPercent = hcUscorePercent.empty() ? 10 : std::stoi(hcUscorePercent);
	auto hcUscoreMult = Game::config->GetValue("hardcore_uscore_enemies_multiplier");
	m_HardcoreUscoreEnemiesMultiplier = hcUscoreMult.empty() ? 2 : std::stoi(hcUscoreMult);
	auto hcDropInv = Game::config->GetValue("hardcore_dropinventory_on_death");
	m_HardcoreDropinventoryOnDeath = hcDropInv.empty() ? false : (hcDropInv == "1");

	// If cloneID is not zero, then hardcore mode is disabled
	// aka minigames and props
	if (Game::zoneManager->GetZoneID().GetCloneID() != 0) m_HardcoreMode = false;
}

Entity* EntityManager::CreateEntity(EntityInfo info, User* user, Entity* parentEntity, const bool controller, const LWOOBJID explicitId) {

	// Determine the objectID for the new entity
	LWOOBJID id;

	// If an explicit ID was provided, use it
	if (explicitId != LWOOBJID_EMPTY) {
		id = explicitId;
	}

	// For non player entites, we'll generate a new ID or set the appropiate flags
	else if (user == nullptr || info.lot != 1) {

		// Entities with no ID already set, often spawned entities, we'll generate a new sequencial ID
		if (info.id == 0) {
			id = ObjectIDManager::GenerateObjectID();
		}

		// Entities with an ID already set, often level entities, we'll use that ID as a base
		else {
			id = info.id;
		}

		// Exclude the zone control object from any flags
		if (!controller && info.lot != 14) {

			// The client flags means the client should render the entity
			GeneralUtils::SetBit(id, eObjectBits::CLIENT);

			// Spawned entities require the spawned flag to render
			if (info.spawnerID != 0) {
				GeneralUtils::SetBit(id, eObjectBits::SPAWNED);
			}
		}
	}

	// For players, we'll use the persistent ID for that character
	else {
		id = user->GetLastUsedChar()->GetObjectID();
	}

	info.id = id;

	Entity* entity = new Entity(id, info, user, parentEntity);

	// Initialize the entity
	entity->Initialize();

	// Add the entity to the entity map
	m_Entities.insert_or_assign(id, entity);

	// Set the zone control entity if the entity is a zone control object, this should only happen once
	if (controller) {
		m_ZoneControlEntity = entity;
	}

	// Check if this entity is a respawn point, if so add it to the registry
	const auto& spawnName = entity->GetVar<std::u16string>(u"respawnname");

	if (!spawnName.empty()) {
		m_SpawnPoints.insert_or_assign(GeneralUtils::UTF16ToWTF8(spawnName), entity->GetObjectID());
	}

	return entity;
}

void EntityManager::DestroyEntity(const LWOOBJID& objectID) {
	DestroyEntity(GetEntity(objectID));
}

void EntityManager::DestroyEntity(Entity* entity) {
	if (!entity) return;

	entity->TriggerEvent(eTriggerEventType::DESTROY, entity);

	const auto id = entity->GetObjectID();

	if (std::count(m_EntitiesToDelete.begin(), m_EntitiesToDelete.end(), id)) {
		return;
	}

	// Destruct networked entities
	if (entity->GetNetworkId() != 0) {
		DestructEntity(entity);
	}

	// Delete this entity at the end of the frame
	ScheduleForDeletion(id);
}

void EntityManager::SerializeEntities() {
	for (size_t i = 0; i < m_EntitiesToSerialize.size(); i++) {
		const LWOOBJID toSerialize = m_EntitiesToSerialize[i];
		auto* entity = GetEntity(toSerialize);

		if (!entity) continue;

		m_SerializationCounter++;

		RakNet::BitStream stream;
		stream.Write<char>(ID_REPLICA_MANAGER_SERIALIZE);
		stream.Write<unsigned short>(entity->GetNetworkId());

		entity->WriteBaseReplicaData(&stream, eReplicaPacketType::SERIALIZATION);
		entity->WriteComponents(&stream, eReplicaPacketType::SERIALIZATION);

		if (entity->GetIsGhostingCandidate()) {
			for (auto* player : PlayerManager::GetAllPlayers()) {
				auto* ghostComponent = player->GetComponent<GhostComponent>();
				if (ghostComponent && ghostComponent->IsObserved(toSerialize)) {
					Game::server->Send(&stream, player->GetSystemAddress(), false);
				}
			}
		} else {
			Game::server->Send(&stream, UNASSIGNED_SYSTEM_ADDRESS, true);
		}
	}
	m_EntitiesToSerialize.clear();
}

void EntityManager::KillEntities() {
	for (size_t i = 0; i < m_EntitiesToKill.size(); i++) {
		const LWOOBJID toKill = m_EntitiesToKill[i];
		auto* entity = GetEntity(toKill);

		if (!entity) {
			LOG("Attempting to kill null entity %llu", toKill);
			continue;
		}

		if (entity->GetScheduledKiller()) {
			entity->Smash(entity->GetScheduledKiller()->GetObjectID(), eKillType::SILENT);
		} else {
			entity->Smash(LWOOBJID_EMPTY, eKillType::SILENT);
		}
	}
	m_EntitiesToKill.clear();
}

void EntityManager::DeleteEntities() {
	for (size_t i = 0; i < m_EntitiesToDelete.size(); i++) {
		const LWOOBJID toDelete = m_EntitiesToDelete[i];
		auto entityToDelete = GetEntity(toDelete);
		if (entityToDelete) {
			// Get all this info first before we delete the player.
			auto networkIdToErase = entityToDelete->GetNetworkId();
			const auto& ghostingToDelete = std::find(m_EntitiesToGhost.begin(), m_EntitiesToGhost.end(), entityToDelete);

			delete entityToDelete;

			entityToDelete = nullptr;

			if (networkIdToErase != 0) m_LostNetworkIds.push(networkIdToErase);

			if (ghostingToDelete != m_EntitiesToGhost.end()) m_EntitiesToGhost.erase(ghostingToDelete);
		} else {
			LOG("Attempted to delete non-existent entity %llu", toDelete);
		}
		m_Entities.erase(toDelete);
	}
	m_EntitiesToDelete.clear();
}

void EntityManager::UpdateEntities(const float deltaTime) {
	for (auto* entity : m_Entities | std::views::values) {
		entity->Update(deltaTime);
	}

	SerializeEntities();
	KillEntities();
	DeleteEntities();
}

Entity* EntityManager::GetEntity(const LWOOBJID& objectId) const {
	const auto& index = m_Entities.find(objectId);

	if (index == m_Entities.end()) {
		return nullptr;
	}

	return index->second;
}

std::vector<Entity*> EntityManager::GetEntitiesInGroup(const std::string& group) {
	std::vector<Entity*> entitiesInGroup;
	for (auto* entity : m_Entities | std::views::values) {
		for (const auto& entityGroup : entity->GetGroups()) {
			if (entityGroup == group) {
				entitiesInGroup.push_back(entity);
			}
		}
	}

	return entitiesInGroup;
}

std::vector<Entity*> EntityManager::GetEntitiesByComponent(const eReplicaComponentType componentType) const {
	std::vector<Entity*> withComp;
	if (componentType != eReplicaComponentType::INVALID) {
		for (auto* entity : m_Entities | std::views::values) {
			if (!entity->HasComponent(componentType)) continue;

			withComp.push_back(entity);
		}
	}
	return withComp;
}

std::vector<Entity*> EntityManager::GetEntitiesByLOT(const LOT& lot) const {
	std::vector<Entity*> entities;

	for (auto* entity : m_Entities | std::views::values) {
		if (entity->GetLOT() == lot) entities.push_back(entity);
	}

	return entities;
}

std::vector<Entity*> EntityManager::GetEntitiesByProximity(NiPoint3 reference, float radius) const {
	std::vector<Entity*> entities;
	if (radius <= 1000.0f) { // The client has a 1000 unit limit on this same logic, so we'll use the same limit
		for (auto* entity : m_Entities | std::views::values) {
			if (NiPoint3::Distance(reference, entity->GetPosition()) <= radius) entities.push_back(entity);
		}
	}
	return entities;
}


Entity* EntityManager::GetZoneControlEntity() const {
	return m_ZoneControlEntity;
}

Entity* EntityManager::GetSpawnPointEntity(const std::string& spawnName) const {
	// Lookup the spawn point entity in the map
	const auto& spawnPoint = m_SpawnPoints.find(spawnName);

	// Check if the spawn point entity is valid just in case
	return spawnPoint == m_SpawnPoints.end() ? nullptr : GetEntity(spawnPoint->second);
}

const std::unordered_map<std::string, LWOOBJID>& EntityManager::GetSpawnPointEntities() const {
	return m_SpawnPoints;
}

void EntityManager::ConstructEntity(Entity* entity, const SystemAddress& sysAddr, const bool skipChecks) {
	if (!entity) {
		LOG("Attempted to construct null entity");
		return;
	}

	if (entity->GetNetworkId() == 0) {
		uint16_t networkId;

		if (!m_LostNetworkIds.empty()) {
			networkId = m_LostNetworkIds.top();
			m_LostNetworkIds.pop();
		} else {
			networkId = ++m_NetworkIdCounter;
		}

		entity->SetNetworkId(networkId);
	}

	if (entity->GetIsGhostingCandidate()) {
		if (std::find(m_EntitiesToGhost.begin(), m_EntitiesToGhost.end(), entity) == m_EntitiesToGhost.end()) {
			m_EntitiesToGhost.push_back(entity);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) {
			CheckGhosting(entity);

			return;
		}
	}

	m_SerializationCounter++;

	RakNet::BitStream stream;

	stream.Write<uint8_t>(ID_REPLICA_MANAGER_CONSTRUCTION);
	stream.Write(true);
	stream.Write<uint16_t>(entity->GetNetworkId());

	entity->WriteBaseReplicaData(&stream, eReplicaPacketType::CONSTRUCTION);
	entity->WriteComponents(&stream, eReplicaPacketType::CONSTRUCTION);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) {
		if (skipChecks) {
			Game::server->Send(&stream, UNASSIGNED_SYSTEM_ADDRESS, true);
		} else {
			for (auto* player : PlayerManager::GetAllPlayers()) {
				if (player->GetPlayerReadyForUpdates()) {
					Game::server->Send(&stream, player->GetSystemAddress(), false);
				} else {
					auto* ghostComponent = player->GetComponent<GhostComponent>();
					if (ghostComponent) ghostComponent->AddLimboConstruction(entity->GetObjectID());
				}
			}
		}
	} else {
		Game::server->Send(&stream, sysAddr, false);
	}

	if (entity->IsPlayer()) {
		if (entity->GetGMLevel() > eGameMasterLevel::CIVILIAN) {
			GameMessages::SendToggleGMInvis(entity->GetObjectID(), true, sysAddr);
		}
	}
}

void EntityManager::ConstructAllEntities(const SystemAddress& sysAddr) {
	//ZoneControl is special:
	ConstructEntity(m_ZoneControlEntity, sysAddr);

	for (auto* entity : m_Entities | std::views::values) {
		if (entity && (entity->GetSpawnerID() != 0 || entity->GetLOT() == 1) && !entity->GetIsGhostingCandidate()) {
			ConstructEntity(entity, sysAddr);
		}
	}

	UpdateGhosting(PlayerManager::GetPlayer(sysAddr));
}

void EntityManager::DestructEntity(Entity* entity, const SystemAddress& sysAddr) {
	if (!entity || entity->GetNetworkId() == 0) return;

	RakNet::BitStream stream;

	stream.Write<uint8_t>(ID_REPLICA_MANAGER_DESTRUCTION);
	stream.Write<uint16_t>(entity->GetNetworkId());

	Game::server->Send(&stream, sysAddr, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);

	for (auto* player : PlayerManager::GetAllPlayers()) {
		if (!player->GetPlayerReadyForUpdates()) {
			auto* ghostComponent = player->GetComponent<GhostComponent>();
			if (ghostComponent) ghostComponent->RemoveLimboConstruction(entity->GetObjectID());
		}
	}
}

void EntityManager::SerializeEntity(Entity* entity) {
	if (!entity || entity->GetNetworkId() == 0) return;

	if (std::find(m_EntitiesToSerialize.begin(), m_EntitiesToSerialize.end(), entity->GetObjectID()) == m_EntitiesToSerialize.end()) {
		m_EntitiesToSerialize.push_back(entity->GetObjectID());
	}
}

void EntityManager::DestructAllEntities(const SystemAddress& sysAddr) {
	for (auto* entity : m_Entities | std::views::values) {
		DestructEntity(entity, sysAddr);
	}
}

void EntityManager::SetGhostDistanceMax(float value) {
	m_GhostDistanceMaxSquared = value * value;
}

void EntityManager::SetGhostDistanceMin(float value) {
	m_GhostDistanceMinSqaured = value * value;
}

void EntityManager::QueueGhostUpdate(LWOOBJID playerID) {
	if (std::find(m_PlayersToUpdateGhosting.begin(), m_PlayersToUpdateGhosting.end(), playerID) == m_PlayersToUpdateGhosting.end()) {
		m_PlayersToUpdateGhosting.push_back(playerID);
	}
}

void EntityManager::UpdateGhosting() {
	for (const auto playerID : m_PlayersToUpdateGhosting) {
		auto* player = PlayerManager::GetPlayer(playerID);

		if (player == nullptr) {
			continue;
		}

		UpdateGhosting(player);
	}

	m_PlayersToUpdateGhosting.clear();
}

void EntityManager::UpdateGhosting(Entity* player) {
	if (!player) return;

	auto* missionComponent = player->GetComponent<MissionComponent>();
	auto* ghostComponent = player->GetComponent<GhostComponent>();

	if (!missionComponent || !ghostComponent) return;

	const auto& referencePoint = ghostComponent->GetGhostReferencePoint();
	const auto isOverride = ghostComponent->GetGhostOverride();

	for (auto* entity : m_EntitiesToGhost) {
		const auto& entityPoint = entity->GetPosition();

		const auto id = entity->GetObjectID();

		const auto observed = ghostComponent->IsObserved(id);

		const auto distance = NiPoint3::DistanceSquared(referencePoint, entityPoint);

		auto ghostingDistanceMax = m_GhostDistanceMaxSquared;
		auto ghostingDistanceMin = m_GhostDistanceMinSqaured;

		const auto isAudioEmitter = entity->GetLOT() == 6368; // https://explorer.lu/objects/6368
		if (isAudioEmitter) {
			ghostingDistanceMax = ghostingDistanceMin;
		}

		auto condition = ServerPreconditions::CheckPreconditions(entity, player);

		if (observed && ((distance > ghostingDistanceMax && !isOverride) || !condition)) {
			ghostComponent->GhostEntity(id);

			DestructEntity(entity, player->GetSystemAddress());

			entity->SetObservers(entity->GetObservers() - 1);
		} else if (!observed && ghostingDistanceMin > distance && condition) {
			// Check collectables, don't construct if it has been collected
			uint32_t collectionId = entity->GetCollectibleID();

			if (collectionId != 0) {
				collectionId = static_cast<uint32_t>(collectionId) + static_cast<uint32_t>(Game::server->GetZoneID() << 8);

				if (missionComponent->HasCollectible(collectionId)) {
					continue;
				}
			}

			ghostComponent->ObserveEntity(id);

			ConstructEntity(entity, player->GetSystemAddress());

			entity->SetObservers(entity->GetObservers() + 1);
		}
	}
}

void EntityManager::CheckGhosting(Entity* entity) {
	if (entity == nullptr) {
		return;
	}

	const auto& referencePoint = entity->GetPosition();

	for (auto* player : PlayerManager::GetAllPlayers()) {
		auto* ghostComponent = player->GetComponent<GhostComponent>();
		if (!ghostComponent) continue;

		const auto& entityPoint = ghostComponent->GetGhostReferencePoint();

		const auto id = entity->GetObjectID();

		const auto observed = ghostComponent->IsObserved(id);

		const auto distance = NiPoint3::DistanceSquared(referencePoint, entityPoint);

		const auto precondition = ServerPreconditions::CheckPreconditions(entity, player);

		if (observed && (distance > ghostingDistanceMax || !precondition)) {
			ghostComponent->GhostEntity(id);

			DestructEntity(entity, player->GetSystemAddress());

			entity->SetObservers(entity->GetObservers() - 1);
		} else if (!observed && (ghostingDistanceMin > distance && precondition)) {
			ghostComponent->ObserveEntity(id);

			ConstructEntity(entity, player->GetSystemAddress());

			entity->SetObservers(entity->GetObservers() + 1);
		}
	}
}

Entity* EntityManager::GetGhostCandidate(LWOOBJID id) const {
	for (auto* entity : m_EntitiesToGhost) {
		if (entity->GetObjectID() == id) {
			return entity;
		}
	}

	return nullptr;
}

bool EntityManager::GetGhostingEnabled() const {
	return m_GhostingEnabled;
}

void EntityManager::ScheduleForKill(Entity* entity) {
	// Deactivate switches if they die
	if (!entity)
		return;

	SwitchComponent* switchComp = entity->GetComponent<SwitchComponent>();
	if (switchComp) {
		entity->TriggerEvent(eTriggerEventType::DEACTIVATED, entity);
	}

	const auto objectId = entity->GetObjectID();

	if (std::find(m_EntitiesToKill.begin(), m_EntitiesToKill.end(), objectId) != m_EntitiesToKill.end()) {
		m_EntitiesToKill.push_back(objectId);
	}
}

void EntityManager::ScheduleForDeletion(LWOOBJID entity) {
	if (std::find(m_EntitiesToDelete.begin(), m_EntitiesToDelete.end(), entity) != m_EntitiesToDelete.end()) {
		m_EntitiesToDelete.push_back(entity);
	}
}


void EntityManager::FireEventServerSide(Entity* origin, std::string args) {
	for (const auto entity : m_Entities | std::views::values) {
		if (entity) {
			entity->OnFireEventServerSide(origin, args);
		}
	}
}

bool EntityManager::IsExcludedFromGhosting(LOT lot) {
	return std::find(m_GhostingExcludedLOTs.begin(), m_GhostingExcludedLOTs.end(), lot) != m_GhostingExcludedLOTs.end();
}
