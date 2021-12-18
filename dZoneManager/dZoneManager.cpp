#include "Game.h"
#include "dCommonVars.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "dLogger.h"
#include "dConfig.h"
#include "InventoryComponent.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "VanityUtilities.h"
#include <chrono>

#include "../dWorldServer/ObjectIDManager.h"

dZoneManager* dZoneManager::m_Address = nullptr;

void dZoneManager::Initialize(const LWOZONEID& zoneID) {
	Game::logger->Log("dZoneManager", "Preparing zone: %i/%i/%i\n", zoneID.GetMapID(), zoneID.GetInstanceID(), zoneID.GetCloneID());
    
    int64_t startTime = 0;
    int64_t endTime = 0;

    startTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    LoadZone(zoneID);

	LOT zoneControlTemplate = 2365;

	std::stringstream query;
	auto result = CDClientDatabase::ExecuteQuery("SELECT zoneControlTemplate, ghostdistance_min, ghostdistance, PlayerLoseCoinsOnDeath FROM ZoneTable WHERE zoneID = " + std::to_string(zoneID.GetMapID()));

	if (!result.eof()) {
		zoneControlTemplate = result.getIntField("zoneControlTemplate", 2365);
		const auto min = result.getIntField("ghostdistance_min", 100);
		const auto max = result.getIntField("ghostdistance", 100);
		EntityManager::Instance()->SetGhostDistanceMax(max + min);
		EntityManager::Instance()->SetGhostDistanceMin(max);
        m_PlayerLoseCoinsOnDeath = (result.getIntField("PlayerLoseCoinsOnDeath", 0) != 0);
	}

	result.finalize();

	Game::logger->Log("dZoneManager", "Creating zone control object %i\n", zoneControlTemplate);

	// Create ZoneControl object
	EntityInfo info;
	info.lot = zoneControlTemplate;
	info.id = 70368744177662;
	Entity* zoneControl = EntityManager::Instance()->CreateEntity(info, nullptr, nullptr, true);
	m_ZoneControlObject = zoneControl;

	m_pZone->Initalize();

    endTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	
    Game::logger->Log("dZoneManager", "Zone prepared in: %llu ms\n", (endTime - startTime));

	VanityUtilities::SpawnVanity();
}

dZoneManager::~dZoneManager() {
	if (m_pZone) delete m_pZone;
    
    for (std::pair<LWOOBJID, Spawner*> p : m_Spawners) {
        if (p.second) {
            delete p.second;
            p.second = nullptr;
        }
        
        m_Spawners.erase(p.first);
    }
}

Zone * dZoneManager::GetZone() {
	return m_pZone;
}

void dZoneManager::LoadZone(const LWOZONEID& zoneID) {
	if (m_pZone) delete m_pZone;

	m_ZoneID = zoneID;
	m_pZone = new Zone(zoneID.GetMapID(), zoneID.GetInstanceID(), zoneID.GetCloneID());
}

void dZoneManager::NotifyZone(const dZoneNotifier & notifier, const LWOOBJID& objectID) {
	switch (notifier) {
	case dZoneNotifier::SpawnedObjectDestroyed:
		break;
	case dZoneNotifier::SpawnedChildObjectDestroyed:
		break;
	case dZoneNotifier::ReloadZone:
        Game::logger->Log("dZoneManager", "Forcing reload of zone %i\n", m_ZoneID.GetMapID());
		LoadZone(m_ZoneID);

		m_pZone->Initalize();
		break;
	case dZoneNotifier::UserJoined:
		break;
	case dZoneNotifier::UserMoved:
		break;
	case dZoneNotifier::PrintAllGameObjects:
		m_pZone->PrintAllGameObjects();
		break;
	case dZoneNotifier::InvalidNotifier:
		Game::logger->Log("dZoneManager", "Got an invalid zone notifier.\n");
		break;
	default:
		Game::logger->Log("dZoneManager", "Unknown zone notifier: %i\n", int(notifier));
	}
}

void dZoneManager::AddSpawner(LWOOBJID id, Spawner* spawner)
{
	m_Spawners.insert_or_assign(id, spawner);
}

LWOZONEID dZoneManager::GetZoneID() const
{
	return m_ZoneID;
}

void dZoneManager::Update(float deltaTime) {
	for (auto spawner : m_Spawners) {
		spawner.second->Update(deltaTime);
	}
}

LWOOBJID dZoneManager::MakeSpawner(SpawnerInfo info)
{
	auto objectId = info.spawnerID;
	
	if (objectId == LWOOBJID_EMPTY)
	{
		objectId = ObjectIDManager::Instance()->GenerateObjectID();

		objectId = GeneralUtils::SetBit(objectId, OBJECT_BIT_CLIENT);

		info.spawnerID = objectId;
	}
	
	auto* spawner = new Spawner(info);

	EntityInfo entityInfo{};

	entityInfo.id = objectId;
	entityInfo.lot = 176;
	
	auto* entity = EntityManager::Instance()->CreateEntity(entityInfo, nullptr, nullptr, false, objectId);

	EntityManager::Instance()->ConstructEntity(entity);
	
	AddSpawner(objectId, spawner);

	return objectId;
}

Spawner* dZoneManager::GetSpawner(const LWOOBJID id)
{
	const auto& index = m_Spawners.find(id);

	if (index == m_Spawners.end()) {
		return nullptr;
	}

	return index->second;
}

void dZoneManager::RemoveSpawner(const LWOOBJID id)
{
	auto* spawner = GetSpawner(id);

	if (spawner == nullptr) {
		Game::logger->Log("dZoneManager", "Failed to find spawner (%llu)\n", id);
		return;
	}

	auto* entity = EntityManager::Instance()->GetEntity(id);

	if (entity != nullptr) {
		entity->Kill();
	}
	else {

		Game::logger->Log("dZoneManager", "Failed to find spawner entity (%llu)\n", id);
	}

	for (auto* node : spawner->m_Info.nodes)
	{
		for (const auto& element : node->entities)
		{
			auto* nodeEntity = EntityManager::Instance()->GetEntity(element);

			if (nodeEntity == nullptr) continue;

			nodeEntity->Kill();
		}

		node->entities.clear();
	}

	spawner->Deactivate();

	Game::logger->Log("dZoneManager", "Destroying spawner (%llu)\n", id);
	
	m_Spawners.erase(id);
	
	delete spawner;
}


std::vector<Spawner*> dZoneManager::GetSpawnersByName(std::string spawnerName) {
	std::vector<Spawner*> spawners;
	for (const auto& spawner : m_Spawners) {
		if (spawner.second->GetName() == spawnerName) {
			spawners.push_back(spawner.second);
		}
	}

	return spawners;
}

std::vector<Spawner*> dZoneManager::GetSpawnersInGroup(std::string group) {
	std::vector<Spawner*> spawnersInGroup;
	for (auto spawner : m_Spawners) {
		for (std::string entityGroup : spawner.second->m_Info.groups) {
			if (entityGroup == group) {
				spawnersInGroup.push_back(spawner.second);
			}
		}
	}

	return spawnersInGroup;
}
