#include "Spawner.h"
#include "EntityManager.h"
#include "dLogger.h"
#include "Game.h"
#include <sstream>
#include <functional>
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "SpawnPatterns.h"

std::map<LOT, std::vector<std::pair<NiPoint3, float>>> Spawner::m_Ratings;

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
		std::vector<Entity*> spawnSmashEntities = Game::entityManager->GetEntitiesInGroup(m_Info.spawnOnSmashGroupName);
		std::vector<Spawner*> spawnSmashSpawners = Game::zoneManager->GetSpawnersInGroup(m_Info.spawnOnSmashGroupName);
		std::vector<Spawner*> spawnSmashSpawnersN = Game::zoneManager->GetSpawnersByName(m_Info.spawnOnSmashGroupName);
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

	m_SpawnPattern = SpawnPatterns::FindSpawnPatterns(m_Info.templateID);

	m_LotsToCheck.push_back(m_Info.templateID);
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

		bool usedSpawnPattern = false;
		
		if (m_SpawnPattern != nullptr) {
			auto pattern = m_SpawnPattern->GetSpawnPatterns();

			// Check the area rating
			// std::map<LOT, std::vector<std::pair<NiPoint3, float>>> m_Ratings
			for (const auto& lot : m_LotsToCheck)
			{
				const auto& it = m_Ratings.find(lot);

				int32_t rating = 0;

				if (it != m_Ratings.end()) {
					// Check if we are within 50units of a rating
					for (const auto& ratingIt : it->second)
					{
						if (NiPoint3::DistanceSquared(ratingIt.first, m_EntityInfo.pos) <= 100.0f * 100.0f)
						{
							rating = ratingIt.second;
							break;
						}
					}
				}

				for (const auto& it : pattern)
				{
					if (it.first > rating) continue;

					// Random number between 0 and 1
					float random = GeneralUtils::GenerateRandomNumber<float>(0, 1);

					const auto& change = it.second.first;

					if (random >= change) continue;

					usedSpawnPattern = true;

					Entity* first = nullptr;

					for (const auto& spawn : it.second.second)
					{
						float angle = GeneralUtils::GenerateRandomNumber<float>(0, 360) * M_PI / 180.0f;
						float radius = GeneralUtils::GenerateRandomNumber<float>(0, 6);

						float x = radius * cos(angle);
						float z = radius * sin(angle);

						auto copy = m_EntityInfo;
						copy.pos.x += x;
						copy.pos.z += z;
						copy.lot = spawn;

						if (std::find(m_LotsToCheck.begin(), m_LotsToCheck.end(), spawn) == m_LotsToCheck.end()) {
							m_LotsToCheck.push_back(spawn);
						}

						Entity* rezdE = Game::entityManager->CreateEntity(copy, nullptr);

						rezdE->GetGroups() = m_Info.groups;

						Game::entityManager->ConstructEntity(rezdE);

						m_Entities.insert({ rezdE->GetObjectID(), spawnNode });

						spawnNode->entities.push_back(rezdE->GetObjectID());

						for (const auto& cb : m_EntitySpawnedCallbacks) {
							cb(rezdE);
						}

						if (first == nullptr) {
							first = rezdE;
						}

						break;
					}

					usedSpawnPattern = true;

					if (m_Entities.size() == m_Info.amountMaintained) {
						m_NeedsUpdate = false;
					}
					
					return first;
				}
			}
		}

		Entity* rezdE = Game::entityManager->CreateEntity(m_EntityInfo, nullptr);

		rezdE->GetGroups() = m_Info.groups;

		Game::entityManager->ConstructEntity(rezdE);

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

	const auto& lot = m_Info.templateID;

	// Add to area rating
	// std::map<LOT, std::vector<std::pair<NiPoint3, float>>> m_Ratings
	// First check if the lot is in the map, if not, add it
	// Than check if there exist any ratings for that lot within 50units of the spawner
	// If there is, add 1 to the rating
	// If there isn't, add a new rating
	const auto& pos = node->position;

	const auto& it2 = m_Ratings.find(lot);

	if (it2 == m_Ratings.end()) {
		m_Ratings.insert({ lot, { { pos, 1.0f } } });
	} else {
		auto& ratings = it2->second;

		bool found = false;
		for (auto& rating : ratings) {
			if (NiPoint3::DistanceSquared(rating.first, pos) < 100.0f * 100.0f) {
				rating.second += 1.0f;

				Game::logger->Log("Spawner", "Rating %f", rating.second);
				found = true;
				break;
			}
		}

		if (!found) {
			ratings.push_back({ pos, 1.0f });
		}
	}
}

void Spawner::UpdateRatings(float deltaTime) {
	// Loop through all ratings and decrease them by deltaTime
	for (auto& rating : m_Ratings) {
		for (auto& rating2 : rating.second) {
			rating2.second -= deltaTime * 0.1f;
		}
	}

	// Loop through all ratings and remove any that are 0 or less
	for (auto it = m_Ratings.begin(); it != m_Ratings.end();) {
		for (auto it2 = it->second.begin(); it2 != it->second.end();) {
			if (it2->second <= 0.0f) {
				it2 = it->second.erase(it2);
			} else {
				++it2;
			}
		}

		if (it->second.empty()) {
			it = m_Ratings.erase(it);
		} else {
			++it;
		}
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
