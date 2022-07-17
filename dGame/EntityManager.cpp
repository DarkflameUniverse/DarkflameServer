#include "EntityManager.h"
#include "RakNetTypes.h"
#include "Game.h"
#include "User.h"
#include "../dWorldServer/ObjectIDManager.h"
#include "Character.h"
#include "GeneralUtils.h"
#include "dServer.h"
#include "Spawner.h"
#include "Player.h"
#include "SkillComponent.h"
#include "SwitchComponent.h"
#include "UserManager.h"
#include "PacketUtils.h"
#include "Metrics.hpp"
#include "dZoneManager.h"
#include "MissionComponent.h"
#include "Game.h"
#include "dLogger.h"

EntityManager* EntityManager::m_Address = nullptr;

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
	// NT - Pipes
	9524,
	12408,

	// AG - Footrace
	4967
};

void EntityManager::Initialize() {
	// Check if this zone has ghosting enabled
	m_GhostingEnabled = std::find(
		m_GhostingExcludedZones.begin(),
		m_GhostingExcludedZones.end(),
		dZoneManager::Instance()->GetZoneID().GetMapID()
	) == m_GhostingExcludedZones.end();
}

EntityManager::~EntityManager() {
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
			id = ObjectIDManager::Instance()->GenerateObjectID();
		}

		// Entities with an ID already set, often level entities, we'll use that ID as a base
        else {
			id = info.id;
		}

		// Exclude the zone control object from any flags
		if(!controller && info.lot != 14) {

			// The client flags means the client should render the entity
			id = GeneralUtils::SetBit(id, OBJECT_BIT_CLIENT);

			// Spawned entities require the spawned flag to render
			if (info.spawnerID != 0) {
				id = GeneralUtils::SetBit(id, OBJECT_BIT_SPAWNED);
			}
		}
    }

	// For players, we'll use the persistent ID for that character
	else {
		id = user->GetLastUsedChar()->GetObjectID();
	}

    info.id = id;
	
	Entity* entity;

	// Check if the entitty if a player, in case use the extended player entity class
	if (user != nullptr) {
		entity = new Player(id, info, user, parentEntity);
	}
	else {
		entity = new Entity(id, info, parentEntity);
	}

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
	if (entity == nullptr) {
		return;
	}

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

void EntityManager::UpdateEntities(const float deltaTime) {
	for (const auto& e : m_Entities) {
	    e.second->Update(deltaTime);
	}

	for (const auto entityId : m_EntitiesToSerialize)
	{
		auto* entity = GetEntity(entityId);

		if (entity == nullptr) continue;

		m_SerializationCounter++;

		RakNet::BitStream stream;

		stream.Write(static_cast<char>(ID_REPLICA_MANAGER_SERIALIZE));
		stream.Write(static_cast<unsigned short>(entity->GetNetworkId()));

		entity->WriteBaseReplicaData(&stream, PACKET_TYPE_SERIALIZATION);
		entity->WriteComponents(&stream, PACKET_TYPE_SERIALIZATION);

		if (entity->GetIsGhostingCandidate())
		{
			for (auto* player : Player::GetAllPlayers())
			{
				if (player->IsObserved(entityId))
				{
					Game::server->Send(&stream, player->GetSystemAddress(), false);
				}
			}
		}
		else
		{
			Game::server->Send(&stream, UNASSIGNED_SYSTEM_ADDRESS, true);
		}
	}

	m_EntitiesToSerialize.clear();

	for (const auto& entry : m_EntitiesToKill)
	{
		auto* entity = GetEntity(entry);

		if (!entity) continue;

		if (entity->GetScheduledKiller())
		{
			entity->Smash(entity->GetScheduledKiller()->GetObjectID(), SILENT);
		}
		else
		{
			entity->Smash(LWOOBJID_EMPTY, SILENT); 
		}
	}

	m_EntitiesToKill.clear();

	for (const auto entry : m_EntitiesToDelete)
	{
		// Get all this info first before we delete the player.
		auto entityToDelete = GetEntity(entry);

		auto networkIdToErase = entityToDelete->GetNetworkId();

		const auto& ghostingToDelete = std::find(m_EntitiesToGhost.begin(), m_EntitiesToGhost.end(), entityToDelete);

		if (entityToDelete)
		{
			// If we are a player run through the player destructor.
			if (entityToDelete->IsPlayer())
			{
				delete dynamic_cast<Player*>(entityToDelete);
			}
			else
			{
				delete entityToDelete;
			}

			entityToDelete = nullptr;

			if (networkIdToErase != 0)
			{
				m_LostNetworkIds.push(networkIdToErase);
			}
		}

		if (ghostingToDelete != m_EntitiesToGhost.end())
		{
			m_EntitiesToGhost.erase(ghostingToDelete);
		}

		m_Entities.erase(entry);
		
	}

	m_EntitiesToDelete.clear();
}

Entity * EntityManager::GetEntity(const LWOOBJID& objectId) const {
	const auto& index = m_Entities.find(objectId);
	
	if (index == m_Entities.end())
	{
		return nullptr;
	}
    
	return index->second;
}

std::vector<Entity*> EntityManager::GetEntitiesInGroup(const std::string& group) {
	std::vector<Entity*> entitiesInGroup;
	for (const auto& entity : m_Entities) {
		for (const auto& entityGroup : entity.second->GetGroups()) {
			if (entityGroup == group) {
				entitiesInGroup.push_back(entity.second);
			}
		}
	}

	return entitiesInGroup;
}

std::vector<Entity*> EntityManager::GetEntitiesByComponent(const int componentType) const {
	std::vector<Entity*> withComp;
	for (const auto& entity : m_Entities) {
		if (componentType != -1 && !entity.second->HasComponent(componentType)) continue;
		
		withComp.push_back(entity.second);
	}
	return withComp;
}

std::vector<Entity *> EntityManager::GetEntitiesByLOT(const LOT &lot) const {
    std::vector<Entity*> entities;

    for (const auto& entity : m_Entities) {
        if (entity.second->GetLOT() == lot)
            entities.push_back(entity.second);
    }

    return entities;
}

Entity* EntityManager::GetZoneControlEntity() const
{
	return m_ZoneControlEntity;
}

Entity* EntityManager::GetSpawnPointEntity(const std::string& spawnName) const
{
	// Lookup the spawn point entity in the map
	const auto& spawnPoint = m_SpawnPoints.find(spawnName);

	if (spawnPoint == m_SpawnPoints.end())
	{
		return nullptr;
	}

	// Check if the spawn point entity is valid just in case
	return GetEntity(spawnPoint->second);
}

const std::unordered_map<std::string, LWOOBJID>& EntityManager::GetSpawnPointEntities() const
{
	return m_SpawnPoints;
}

void EntityManager::ConstructEntity(Entity* entity, const SystemAddress& sysAddr, const bool skipChecks) {
	if (entity->GetNetworkId() == 0)
	{
		uint16_t networkId;
		
		if (!m_LostNetworkIds.empty())
		{
			networkId = m_LostNetworkIds.top();
			m_LostNetworkIds.pop();
		}
		else
		{
			networkId = ++m_NetworkIdCounter;
		}

		entity->SetNetworkId(networkId);
	}
	
	const auto checkGhosting = entity->GetIsGhostingCandidate();

	if (checkGhosting)
	{
		const auto& iter = std::find(m_EntitiesToGhost.begin(), m_EntitiesToGhost.end(), entity);

		if (iter == m_EntitiesToGhost.end())
		{
			m_EntitiesToGhost.push_back(entity);
		}
	}

	if (checkGhosting && sysAddr == UNASSIGNED_SYSTEM_ADDRESS)
	{
		CheckGhosting(entity);

		return;
	}

	m_SerializationCounter++;
	
	RakNet::BitStream stream;

	stream.Write(static_cast<char>(ID_REPLICA_MANAGER_CONSTRUCTION));
	stream.Write(true);
	stream.Write(static_cast<unsigned short>(entity->GetNetworkId()));
	
	entity->WriteBaseReplicaData(&stream, PACKET_TYPE_CONSTRUCTION);
	entity->WriteComponents(&stream, PACKET_TYPE_CONSTRUCTION);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS)
	{
		if (skipChecks)
		{
			Game::server->Send(&stream, UNASSIGNED_SYSTEM_ADDRESS, true);
		}
		else
		{
			for (auto* player : Player::GetAllPlayers())
			{
				if (player->GetPlayerReadyForUpdates())
				{
					Game::server->Send(&stream, player->GetSystemAddress(), false);
				}
				else
				{
					player->AddLimboConstruction(entity->GetObjectID());
				}
			}
		}
	}
	else
	{
		Game::server->Send(&stream, sysAddr, false);
	}

	// PacketUtils::SavePacket("[24]_"+std::to_string(entity->GetObjectID()) + "_" + std::to_string(m_SerializationCounter) + ".bin", (char*)stream.GetData(), stream.GetNumberOfBytesUsed());

	if (entity->IsPlayer())
	{
		if (entity->GetGMLevel() > GAME_MASTER_LEVEL_CIVILIAN)
		{
			GameMessages::SendToggleGMInvis(entity->GetObjectID(), true, sysAddr);
		}
	}
}

void EntityManager::ConstructAllEntities(const SystemAddress& sysAddr) {
	//ZoneControl is special:
	ConstructEntity(m_ZoneControlEntity, sysAddr);

    for (const auto& e : m_Entities) {
		if (e.second && (e.second->GetSpawnerID() != 0 || e.second->GetLOT() == 1) && !e.second->GetIsGhostingCandidate()) {
			ConstructEntity(e.second, sysAddr);
		}
    }

	UpdateGhosting(Player::GetPlayer(sysAddr));
}

void EntityManager::DestructEntity(Entity* entity, const SystemAddress& sysAddr) {
	if (entity->GetNetworkId() == 0)
	{
		return;
	}
	
	RakNet::BitStream stream;

	stream.Write(static_cast<char>(ID_REPLICA_MANAGER_DESTRUCTION));
	stream.Write(static_cast<unsigned short>(entity->GetNetworkId()));

	Game::server->Send(&stream, sysAddr, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);

	for (auto* player : Player::GetAllPlayers())
	{
		if (!player->GetPlayerReadyForUpdates())
		{
			player->RemoveLimboConstruction(entity->GetObjectID());
		}
	}
}

void EntityManager::SerializeEntity(Entity* entity) {
	if (entity->GetNetworkId() == 0)
	{
		return;
	}

	if (std::find(m_EntitiesToSerialize.begin(), m_EntitiesToSerialize.end(), entity->GetObjectID()) == m_EntitiesToSerialize.end())
	{
		m_EntitiesToSerialize.push_back(entity->GetObjectID());
	}

	//PacketUtils::SavePacket(std::to_string(m_SerializationCounter) + "_[27]_"+std::to_string(entity->GetObjectID()) + ".bin", (char*)stream.GetData(), stream.GetNumberOfBytesUsed());
}

void EntityManager::DestructAllEntities(const SystemAddress& sysAddr) {
    for (const auto& e : m_Entities) {
		DestructEntity(e.second, sysAddr);
    }
}

void EntityManager::SetGhostDistanceMax(float value) 
{
	m_GhostDistanceMaxSquared = value * value;
}

float EntityManager::GetGhostDistanceMax() const
{
	return std::sqrt(m_GhostDistanceMaxSquared);
}

void EntityManager::SetGhostDistanceMin(float value) 
{
	m_GhostDistanceMinSqaured = value * value;
}

float EntityManager::GetGhostDistanceMin() const
{
	return std::sqrt(m_GhostDistanceMinSqaured);
}

void EntityManager::QueueGhostUpdate(LWOOBJID playerID) 
{
	const auto& iter = std::find(m_PlayersToUpdateGhosting.begin(), m_PlayersToUpdateGhosting.end(), playerID);

	if (iter == m_PlayersToUpdateGhosting.end())
	{
		m_PlayersToUpdateGhosting.push_back(playerID);
	}
}

void EntityManager::UpdateGhosting() 
{
	for (const auto playerID : m_PlayersToUpdateGhosting)
	{
		auto* player = Player::GetPlayer(playerID);

		if (player == nullptr)
		{
			continue;
		}

		UpdateGhosting(player);
	}

	m_PlayersToUpdateGhosting.clear();
}

void EntityManager::UpdateGhosting(Player* player) 
{
	if (player == nullptr)
	{
		return;
	}

	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (missionComponent == nullptr)
	{
		return;
	}

	const auto& referencePoint = player->GetGhostReferencePoint();
	const auto isOverride = player->GetGhostOverride();

	for (auto* entity : m_EntitiesToGhost)
	{
		const auto isAudioEmitter = entity->GetLOT() == 6368;

		const auto& entityPoint = entity->GetPosition();

		const int32_t id = entity->GetObjectID();

		const auto observed = player->IsObserved(id);

		const auto distance = NiPoint3::DistanceSquared(referencePoint, entityPoint);

		auto ghostingDistanceMax = m_GhostDistanceMaxSquared;
		auto ghostingDistanceMin = m_GhostDistanceMinSqaured;

		if (isAudioEmitter)
		{
			ghostingDistanceMax = ghostingDistanceMin;
		}

		if (observed && distance > ghostingDistanceMax && !isOverride)
		{
			player->GhostEntity(id);

			DestructEntity(entity, player->GetSystemAddress());

			entity->SetObservers(entity->GetObservers() - 1);
		}
		else if (!observed && ghostingDistanceMin > distance)
		{
			// Check collectables, don't construct if it has been collected
			uint32_t collectionId = entity->GetCollectibleID();

			if (collectionId != 0)
			{
				collectionId = static_cast<uint32_t>(collectionId) + static_cast<uint32_t>(Game::server->GetZoneID() << 8);

				if (missionComponent->HasCollectible(collectionId))
				{
					continue;
				}
			}

			player->ObserveEntity(id);
			
			ConstructEntity(entity, player->GetSystemAddress());
			
			entity->SetObservers(entity->GetObservers() + 1);
		}
	}
}

void EntityManager::CheckGhosting(Entity* entity) 
{
	if (entity == nullptr)
	{
		return;
	}

	const auto& referencePoint = entity->GetPosition();
	
	auto ghostingDistanceMax = m_GhostDistanceMaxSquared;
	auto ghostingDistanceMin = m_GhostDistanceMinSqaured;

	const auto isAudioEmitter = entity->GetLOT() == 6368;

	for (auto* player : Player::GetAllPlayers())
	{
		const auto& entityPoint = player->GetGhostReferencePoint();

		const int32_t id = entity->GetObjectID();

		const auto observed = player->IsObserved(id);

		const auto distance = NiPoint3::DistanceSquared(referencePoint, entityPoint);

		if (observed && distance > ghostingDistanceMax)
		{
			player->GhostEntity(id);

			DestructEntity(entity, player->GetSystemAddress());

			entity->SetObservers(entity->GetObservers() - 1);
		}
		else if (!observed && ghostingDistanceMin > distance)
		{
			player->ObserveEntity(id);
			
			ConstructEntity(entity, player->GetSystemAddress());
			
			entity->SetObservers(entity->GetObservers() + 1);
		}
	}
}

Entity* EntityManager::GetGhostCandidate(int32_t id) 
{
	for (auto* entity : m_EntitiesToGhost)
	{
		if (entity->GetObjectID() == id)
		{
			return entity;
		}
	}
	
	return nullptr;
}

bool EntityManager::GetGhostingEnabled() const
{
	return m_GhostingEnabled;
}

void EntityManager::ResetFlags() {
	for (const auto& e : m_Entities) {
		e.second->ResetFlags();
	}
}

void EntityManager::ScheduleForKill(Entity* entity) {
	// Deactivate switches if they die
	if (!entity)
		return;
	
	SwitchComponent* switchComp = entity->GetComponent<SwitchComponent>();
	if (switchComp) {
		entity->TriggerEvent("OnDectivated");
	}

	const auto objectId = entity->GetObjectID();

	if (std::count(m_EntitiesToKill.begin(), m_EntitiesToKill.end(), objectId))
	{
		return;
	}

	m_EntitiesToKill.push_back(objectId);
}

void EntityManager::ScheduleForDeletion(LWOOBJID entity) 
{
	if (std::count(m_EntitiesToDelete.begin(), m_EntitiesToDelete.end(), entity))
	{
		return;
	}

	m_EntitiesToDelete.push_back(entity);
}


void EntityManager::FireEventServerSide(Entity* origin, std::string args) {
	for (std::pair<LWOOBJID, Entity*> e : m_Entities) {
		if (e.second) {
			e.second->OnFireEventServerSide(origin, args);
		}
	}
}

bool EntityManager::IsExcludedFromGhosting(LOT lot) 
{
	return std::find(m_GhostingExcludedLOTs.begin(), m_GhostingExcludedLOTs.end(), lot) != m_GhostingExcludedLOTs.end();
}
