#include "Spawner.h"
#include "EntityManager.h"
#include "Logger.h"
#include "Game.h"
#include <sstream>
#include <functional>
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include <algorithm>
#include <ranges>

Spawner::Spawner(const SpawnerInfo& info) {
	m_Info = info;
	m_Active = m_Info.activeOnLoad && info.spawnActivator;
	m_EntityInfo = EntityInfo();
	m_EntityInfo.spawner = this;

	if (!m_Info.emulated) {
		m_EntityInfo.spawnerID = m_Info.spawnerID;
	} else {
		m_EntityInfo.spawnerID = m_Info.emulator;
		m_Info.isNetwork = false;
	}

	m_EntityInfo.lot = m_Info.templateID;
	m_EntityInfo.scale = m_Info.templateScale;

	m_Start = m_Info.noTimedSpawn;

	int timerCount = m_Info.amountMaintained;

	if (m_Info.amountMaintained > m_Info.nodes.size()) {
		timerCount = m_Info.nodes.size();
	}

	for (int i = 0; i < timerCount; ++i) {
		m_WaitTimes.push_back(m_Info.respawnTime);
	}

	if (m_Info.spawnOnSmashGroupName != "") {
		std::vector<Entity*> spawnSmashEntities = Game::entityManager->GetEntitiesInGroup(m_Info.spawnOnSmashGroupName);
		std::vector<Spawner*> spawnSmashSpawners = Game::zoneManager->GetSpawnersInGroup(m_Info.spawnOnSmashGroupName);
		std::vector<Spawner*> spawnSmashSpawnersN = Game::zoneManager->GetSpawnersByName(m_Info.spawnOnSmashGroupName);
		for (Entity* ssEntity : spawnSmashEntities) {
			m_SpawnSmashFoundGroup = true;
			ssEntity->AddDieCallback([=, this]() {
				Spawn();
				});
		}
		for (Spawner* ssSpawner : spawnSmashSpawners) {
			m_SpawnSmashFoundGroup = true;
			ssSpawner->AddSpawnedEntityDieCallback([=, this]() {
				Spawn();
				});
		}
		for (Spawner* ssSpawner : spawnSmashSpawnersN) {
			m_SpawnSmashFoundGroup = true;
			m_SpawnOnSmashID = ssSpawner ? ssSpawner->m_Info.spawnerID : LWOOBJID_EMPTY;
			ssSpawner->AddSpawnedEntityDieCallback([=, this]() {
				// Intentionally left as a non debug log since i have no idea how much stuff this would affect
				LOG("WOULD HAVE SPAWNED %i", m_EntityInfo.lot);
				});
		}
	}
}

Entity* Spawner::Spawn() {
	std::vector<SpawnerNode*> freeNodes;
	for (SpawnerNode* node : m_Info.nodes) {
		if (node->entities.size() < node->nodeMax) {
			freeNodes.push_back(node);
		}
	}

	return Spawn(freeNodes);
}

Entity* Spawner::Spawn(const std::vector<SpawnerNode*>& freeNodes, const bool force) {
	Entity* spawnedEntity = nullptr;
	if (force || ((m_Entities.size() < m_Info.amountMaintained) && !freeNodes.empty() && (m_AmountSpawned < m_Info.maxToSpawn || m_Info.maxToSpawn == -1))) {
		// first sum the weights we were provided
		int32_t spawnWeight = 0;
		for (const auto* const node : freeNodes) spawnWeight += node->weight;
		auto chosenWeight = GeneralUtils::GenerateRandomNumber<int32_t>(1, spawnWeight);

		// Default to 0 incase something goes wrong in this calc
		// Roll the spawner nodes based on their weights, higher weights = more likely to spawn
		SpawnerNode* spawnNode = freeNodes[0];
		for (auto* const node : freeNodes) {
			chosenWeight -= node->weight;
			if (chosenWeight <= 0) {
				spawnNode = node;
				break; // we rolled a spawner
			}
		}

		++m_AmountSpawned;
		m_EntityInfo.pos = spawnNode->position;
		m_EntityInfo.rot = spawnNode->rotation;
		m_EntityInfo.settings = spawnNode->config;
		m_EntityInfo.id = 0;
		m_EntityInfo.spawner = this;

		if (!m_Info.emulated) {
			m_EntityInfo.spawnerNodeID = spawnNode->nodeID;
			m_EntityInfo.hasSpawnerNodeID = true;
			m_EntityInfo.spawnerID = m_Info.spawnerID;
		}

		spawnedEntity = Game::entityManager->CreateEntity(m_EntityInfo, nullptr);

		spawnedEntity->GetGroups() = m_Info.groups;

		Game::entityManager->ConstructEntity(spawnedEntity);

		m_Entities[spawnedEntity->GetObjectID()] = spawnNode;
		spawnNode->entities.push_back(spawnedEntity->GetObjectID());
		if (m_Entities.size() == m_Info.amountMaintained) {
			m_NeedsUpdate = false;
		}

		for (const auto& cb : m_EntitySpawnedCallbacks) {
			cb(spawnedEntity);
		}
	}

	return spawnedEntity;
}

void Spawner::AddSpawnedEntityDieCallback(std::function<void()> callback) {
	m_SpawnedEntityDieCallbacks.push_back(callback);
}

void Spawner::AddEntitySpawnedCallback(std::function<void(Entity*)> callback) {
	m_EntitySpawnedCallbacks.push_back(callback);
}

void Spawner::Reset() {
	m_Start = true;
	DestroyAllEntities();
	m_Entities.clear();
	m_AmountSpawned = 0;
	m_NeedsUpdate = true;
}

void Spawner::DestroyAllEntities(){
	for (auto* node : m_Info.nodes) {
		for (const auto& element : node->entities) {
			auto* entity = Game::entityManager->GetEntity(element);
			if (entity == nullptr) continue;
			entity->Kill();
		}
		node->entities.clear();
	}
}

void Spawner::SoftReset() {
	m_Start = true;
	m_AmountSpawned = 0;
	m_NeedsUpdate = true;
}

void Spawner::SetRespawnTime(const float time) {
	m_Info.respawnTime = time;

	for (size_t i = 0; i < m_WaitTimes.size(); ++i) {
		m_WaitTimes[i] = 0;
	}

	m_Start = true;
	m_NeedsUpdate = true;
}

void Spawner::SetNumToMaintain(const int32_t value) {
	m_Info.amountMaintained = value;
}

void Spawner::Update(const float deltaTime) {
	if (m_Start && m_Active) {
		m_Start = false;

		const auto toSpawn = m_Info.amountMaintained - m_AmountSpawned;
		for (auto i = 0; i < toSpawn; ++i) {
			Spawn();
		}

		m_WaitTimes.clear();

		return;
	}

	if (!m_NeedsUpdate || !m_Active || m_Info.spawnsOnSmash) return;

	for (size_t i = 0; i < m_WaitTimes.size(); ) {
		m_WaitTimes[i] += deltaTime;
		if (m_WaitTimes[i] >= m_Info.respawnTime) {
			m_WaitTimes.erase(m_WaitTimes.begin() + i);

			Spawn();
		} else {
			i++;
		}
	}
}

const std::vector<LWOOBJID> Spawner::GetSpawnedObjectIDs() const {
	std::vector<LWOOBJID> ids;
	ids.reserve(m_Entities.size());
	for (const auto objId : m_Entities | std::views::keys) {
		ids.push_back(objId);
	}
	return ids;
}

void Spawner::NotifyOfEntityDeath(const LWOOBJID& objectID) {
	for (const auto& cb : m_SpawnedEntityDieCallbacks) {
		cb();
	}

	m_NeedsUpdate = true;
	m_WaitTimes.push_back(0.0f);
	SpawnerNode* node;

	auto it = m_Entities.find(objectID);
	if (it != m_Entities.end()) node = it->second;
	else return;

	if (!node) return;

	for (size_t i = 0; i < node->entities.size();) {
		if (node->entities[i] && node->entities[i] == objectID)
			node->entities.erase(node->entities.begin() + i);
		else
			i++;
	}

	m_Entities.erase(objectID);

	auto* const spawnOnSmash = Game::zoneManager->GetSpawner(m_SpawnOnSmashID);
	if (spawnOnSmash) {
		spawnOnSmash->Reset();
	}
}

void Spawner::Activate() {
	m_Active = true;
	m_NeedsUpdate = true;

	for (auto& time : m_WaitTimes) {
		time = 0;
	}
}

void Spawner::SetSpawnLot(const LOT lot) {
	m_EntityInfo.lot = lot;
}
