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
#include "EntityInfo.h"

/**
 *  Any given spawner owns a certain number of spawner nodes
 *  these nodes are where entities are actually spawned
 *  The first spawner nodes waypoint in any given network contains the base config for all the spawner nodes
 *  Then each spawner node after the first may contain duplicate settings which override the base ones
 *  If spawner node 1 has an attached_path of "1", then all spawner nodes in this spawner network will have
 *  an attached_path of "1".
 *  Each spawner node can also specify attached_path of any other value and it will override the one provided by node 1.
 *  If a spawner node does NOT provide an override, the first one will be used
 *  I have no clue why the nodes are pointers, beats me
 *  sn = SpawnerNode
 *       Spawner
 *  ----------------
 *  |          sn   |
 *  |  sn           |
 *  |       sn      |
 *  |               |
 *  |          sn   |
 *  |  sn           |
 *  -----------------
 */
struct SpawnerNode {
	// This spawner nodes position in the world
	NiPoint3 position = NiPoint3Constant::ZERO;
	// The rotation of this spawner in the world
	NiQuaternion rotation = QuatUtils::IDENTITY;
	// This spawners nodes ID in this spawner network
	uint32_t nodeID = 0;
	// The max number of entities that can be spawned by this node
	uint32_t nodeMax = 1;
	// The weight (chance) this spawner node has. Higher is more common
	int32_t weight = 1;
	// The IDs of entities spawned by this spawner node
	std::vector<LWOOBJID> entities;
	// The config of all entities spawned by this node
	LwoNameValue config;
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
	Spawner(const SpawnerInfo& info);

	Entity* Spawn();
	Entity* Spawn(const std::vector<SpawnerNode*>& freeNodes, bool force = false);
	void Update(float deltaTime);
	void NotifyOfEntityDeath(const LWOOBJID& objectID);
	void Activate();
	void Deactivate() { m_Active = false; };
	int32_t GetAmountSpawned() { return m_AmountSpawned; };
	std::string GetName() { return m_Info.name; };
	std::vector<std::string> GetGroups() { return m_Info.groups; };
	void AddSpawnedEntityDieCallback(const std::function<void()> callback);
	void AddEntitySpawnedCallback(const std::function<void(Entity*)> callback);
	void SetSpawnLot(const LOT lot);
	void Reset();
	void DestroyAllEntities();
	void SoftReset();
	void SetRespawnTime(const float time);
	void SetNumToMaintain(const int32_t value);
	bool GetIsSpawnSmashGroup() const { return m_SpawnSmashFoundGroup; };
	const std::vector<LWOOBJID> GetSpawnedObjectIDs() const;

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
	LWOOBJID m_SpawnOnSmashID = LWOOBJID_EMPTY;
};

#endif // SPAWNER_H
