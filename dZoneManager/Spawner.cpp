#include "Spawner.h"
#include "EntityManager.h"
#include "dLogger.h"
#include "Game.h"
#include <sstream>
#include <functional>
#include "GeneralUtils.h"
#include "dZoneManager.h"

Spawner::Spawner(const SpawnerInfo info) {
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

	//ssssh...
	if (m_EntityInfo.lot == 14718) { //AG - MAELSTROM SAMPLE
		m_Info.groups.emplace_back("MaelstromSamples");
	}

	if (m_EntityInfo.lot == 14375) //AG - SPIDER BOSS EGG
	{
		m_Info.groups.emplace_back("EGG");
	}

	int timerCount = m_Info.amountMaintained;

	if (m_Info.amountMaintained > m_Info.nodes.size()) {
		timerCount = m_Info.nodes.size();
	}

	for (int i = 0; i < timerCount; ++i) {
		m_WaitTimes.push_back(m_Info.respawnTime);
	}

	if (m_Info.spawnOnSmashGroupName != "") {
		std::vector<Entity*> spawnSmashEntities = EntityManager::Instance()->GetEntitiesInGroup(m_Info.spawnOnSmashGroupName);
		std::vector<Spawner*> spawnSmashSpawners = dZoneManager::Instance()->GetSpawnersInGroup(m_Info.spawnOnSmashGroupName);
		std::vector<Spawner*> spawnSmashSpawnersN = dZoneManager::Instance()->GetSpawnersByName(m_Info.spawnOnSmashGroupName);
		for (Entity* ssEntity : spawnSmashEntities) {
			m_SpawnSmashFoundGroup = true;
			ssEntity->AddDieCallback([=]() {
				Spawn();
				});
		}
		for (Spawner* ssSpawner : spawnSmashSpawners) {
			m_SpawnSmashFoundGroup = true;
			ssSpawner->AddSpawnedEntityDieCallback([=]() {
				Spawn();
				});
		}
		for (Spawner* ssSpawner : spawnSmashSpawnersN) {
			m_SpawnSmashFoundGroup = true;
			m_SpawnOnSmash = ssSpawner;
			ssSpawner->AddSpawnedEntityDieCallback([=]() {
				Spawn();
				});
		}
	}
}

Spawner::~Spawner() {

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

Entity* Spawner::Spawn(std::vector<SpawnerNode*> freeNodes, const bool force) {
	if (force || ((m_Entities.size() < m_Info.amountMaintained) && (freeNodes.size() > 0) && (m_AmountSpawned < m_Info.maxToSpawn || m_Info.maxToSpawn == -1))) {
		SpawnerNode* spawnNode = freeNodes[GeneralUtils::GenerateRandomNumber<int>(0, freeNodes.size() - 1)];
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

		Entity* rezdE = EntityManager::Instance()->CreateEntity(m_EntityInfo, nullptr);

		rezdE->GetGroups() = m_Info.groups;

		EntityManager::Instance()->ConstructEntity(rezdE);

		m_Entities.insert({ rezdE->GetObjectID(), spawnNode });
		spawnNode->entities.push_back(rezdE->GetObjectID());
		if (m_Entities.size() == m_Info.amountMaintained) {
			m_NeedsUpdate = false;
		}

		for (const auto& cb : m_EntitySpawnedCallbacks) {
			cb(rezdE);
		}

		return rezdE;
	}

	return nullptr;
}

void Spawner::AddSpawnedEntityDieCallback(std::function<void()> callback) {
	m_SpawnedEntityDieCallbacks.push_back(callback);
}

void Spawner::AddEntitySpawnedCallback(std::function<void(Entity*)> callback) {
	m_EntitySpawnedCallbacks.push_back(callback);
}

void Spawner::Reset() {
	m_Start = true;

	for (auto* node : m_Info.nodes) {
		for (const auto& spawned : node->entities) {
			auto* entity = EntityManager::Instance()->GetEntity(spawned);

			if (entity == nullptr) continue;

			entity->Kill();
		}

		node->entities.clear();
	}

	m_Entities.clear();
	m_AmountSpawned = 0;
	m_NeedsUpdate = true;
}

void Spawner::SoftReset() {
	m_Start = true;
	m_AmountSpawned = 0;
	m_NeedsUpdate = true;
}

void Spawner::SetRespawnTime(float time) {
	m_Info.respawnTime = time;

	for (size_t i = 0; i < m_WaitTimes.size(); ++i) {
		m_WaitTimes[i] = 0;
	};

	m_Start = true;
	m_NeedsUpdate = true;
}

void Spawner::SetNumToMaintain(int32_t value) {
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

	if (!m_NeedsUpdate) return;
	if (!m_Active) return;
	//if (m_Info.noTimedSpawn) return;
	if (m_Info.spawnsOnSmash) {
		if (!m_SpawnSmashFoundGroup) {

		}
		return;
	}
	for (size_t i = 0; i < m_WaitTimes.size(); ++i) {
		m_WaitTimes[i] += deltaTime;
		if (m_WaitTimes[i] >= m_Info.respawnTime) {
			m_WaitTimes.erase(m_WaitTimes.begin() + i);

			Spawn();
		}
	}
}

void Spawner::NotifyOfEntityDeath(const LWOOBJID& objectID) {
	for (std::function<void()> cb : m_SpawnedEntityDieCallbacks) {
		cb();
	}

	m_NeedsUpdate = true;
	//m_RespawnTime = 10.0f;
	m_WaitTimes.push_back(0.0f);
	SpawnerNode* node;

	auto it = m_Entities.find(objectID);
	if (it != m_Entities.end()) node = it->second;
	else return;

	if (!node) {
		return;
	}

	for (size_t i = 0; i < node->entities.size(); ++i) {
		if (node->entities[i] && node->entities[i] == objectID)
			node->entities.erase(node->entities.begin() + i);
	}

	m_Entities.erase(objectID);

	if (m_SpawnOnSmash != nullptr) {
		m_SpawnOnSmash->Reset();
	}
}

void Spawner::Activate() {
	m_Active = true;
	m_NeedsUpdate = true;

	for (auto& time : m_WaitTimes) {
		time = 0;
	}
}

void Spawner::SetSpawnLot(LOT lot) {
	m_EntityInfo.lot = lot;
}
