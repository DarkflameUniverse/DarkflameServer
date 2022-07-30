#ifndef SPAWNER_H
#define SPAWNER_H

#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Entity.h"
#include "dZMCommon.h"
#include <vector>
#include <string>
#include <functional>
#include "LDFFormat.h"

struct SpawnerNode {
	NiPoint3 position = NiPoint3::ZERO;
	NiQuaternion rotation = NiQuaternion::IDENTITY;
	uint32_t nodeID = 0;
	uint32_t nodeMax = 1;
	std::vector<LWOOBJID> entities;
	std::vector<LDFBaseData*> config;
};

struct SpawnerInfo {
	LWOOBJID spawnerID = LWOOBJID_EMPTY;
	LOT templateID = -1;
	std::string name = "";
	float templateScale = 1;
	float respawnTime = 0.0f;
	int32_t maxToSpawn = -1;
	uint32_t amountMaintained = 1;
	bool activeOnLoad = true;
	std::vector<SpawnerNode*> nodes = {};
	bool isNetwork = false;
	bool spawnsOnSmash = false;
	std::string spawnOnSmashGroupName = "";
	std::vector<std::string> groups = {};
	bool noAutoSpawn = false;
	bool noTimedSpawn = false;
	std::string grpNameQBShowBricks = "";
	bool spawnActivator = true;

	bool emulated = false;
	LWOOBJID emulator = LWOOBJID_EMPTY;
};

class Spawner {
public:
	Spawner(SpawnerInfo info);
	~Spawner();

	Entity* Spawn();
	Entity* Spawn(std::vector<SpawnerNode*> freeNodes, bool force = false);
	void Update(float deltaTime);
	void NotifyOfEntityDeath(const LWOOBJID& objectID);
	void Activate();
	void Deactivate() { m_Active = false; };
	int32_t GetAmountSpawned() { return m_AmountSpawned; };
	std::string GetName() { return m_Info.name; };
	std::vector<std::string> GetGroups() { return m_Info.groups; };
	void AddSpawnedEntityDieCallback(std::function<void()> callback);
	void AddEntitySpawnedCallback(std::function<void(Entity*)> callback);
	void SetSpawnLot(LOT lot);
	void Reset();
	void SoftReset();
	void SetRespawnTime(float time);
	void SetNumToMaintain(int32_t value);
	bool GetIsSpawnSmashGroup() const { return m_SpawnSmashFoundGroup; };

	SpawnerInfo m_Info;
	bool m_Active = true;
private:
	std::vector<std::function<void()>> m_SpawnedEntityDieCallbacks = {};
	std::vector<std::function<void(Entity*)>> m_EntitySpawnedCallbacks = {};


	bool m_SpawnSmashFoundGroup = false;
	std::vector<float> m_WaitTimes = {};
	bool m_NeedsUpdate = true;
	std::map<LWOOBJID, SpawnerNode*> m_Entities = {};
	EntityInfo m_EntityInfo;
	int32_t m_AmountSpawned = 0;
	bool m_Start = false;
	Spawner* m_SpawnOnSmash = nullptr;
};

#endif // SPAWNER_H
