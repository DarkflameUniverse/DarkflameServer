#ifndef ENTITYMANAGER_H
#define ENTITYMANAGER_H

#include <map>
#include <stack>
#include <vector>
#include <unordered_map>

#include "dCommonVars.h"

class Entity;
class EntityInfo;
class Player;
class User;
enum class eReplicaComponentType : uint32_t;

struct SystemAddress;

class EntityManager {
public:
	void Initialize();

	void UpdateEntities(float deltaTime);
	Entity* CreateEntity(EntityInfo info, User* user = nullptr, Entity* parentEntity = nullptr, bool controller = false, LWOOBJID explicitId = LWOOBJID_EMPTY);
	void DestroyEntity(const LWOOBJID& objectID);
	void DestroyEntity(Entity* entity);
	Entity* GetEntity(const LWOOBJID& objectId) const;
	std::vector<Entity*> GetEntitiesInGroup(const std::string& group);
	std::vector<Entity*> GetEntitiesByComponent(eReplicaComponentType componentType) const;
	std::vector<Entity*> GetEntitiesByLOT(const LOT& lot) const;
	Entity* GetZoneControlEntity() const;

	// Get spawn point entity by spawn name
	Entity* GetSpawnPointEntity(const std::string& spawnName) const;

	// Get spawn points
	const std::unordered_map<std::string, LWOOBJID>& GetSpawnPointEntities() const;

	//To make it obvious this SHOULD NOT be used outside of debug:
#ifdef _DEBUG
	const std::unordered_map<LWOOBJID, Entity*> GetAllEntities() const { return m_Entities; }
#endif

	void ConstructEntity(Entity* entity, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS, bool skipChecks = false);
	void DestructEntity(Entity* entity, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);
	void SerializeEntity(Entity* entity);

	void ConstructAllEntities(const SystemAddress& sysAddr);
	void DestructAllEntities(const SystemAddress& sysAddr);

	void SetGhostDistanceMax(float value);
	float GetGhostDistanceMax() const;
	void SetGhostDistanceMin(float value);
	float GetGhostDistanceMin() const;
	void QueueGhostUpdate(LWOOBJID playerID);
	void UpdateGhosting();
	void UpdateGhosting(Player* player);
	void CheckGhosting(Entity* entity);
	Entity* GetGhostCandidate(int32_t id);
	bool GetGhostingEnabled() const;

	void ResetFlags();

	void ScheduleForKill(Entity* entity);

	void ScheduleForDeletion(LWOOBJID entity);

	void FireEventServerSide(Entity* origin, std::string args);

	static bool IsExcludedFromGhosting(LOT lot);

	const bool GetHardcoreMode() { return m_HardcoreMode; };
	const uint32_t GetHardcoreLoseUscoreOnDeathPercent() { return m_HardcoreLoseUscoreOnDeathPercent; };
	const bool GetHardcoreDropinventoryOnDeath() { return m_HardcoreDropinventoryOnDeath; };
	const uint32_t GetHardcoreUscoreEnemiesMultiplier() { return m_HardcoreUscoreEnemiesMultiplier; };

private:
	void SerializeEntities();
	void KillEntities();
	void DeleteEntities();

	static std::vector<LWOMAPID> m_GhostingExcludedZones;
	static std::vector<LOT> m_GhostingExcludedLOTs;

	std::unordered_map<LWOOBJID, Entity*> m_Entities;
	std::vector<LWOOBJID> m_EntitiesToKill;
	std::vector<LWOOBJID> m_EntitiesToDelete;
	std::vector<LWOOBJID> m_EntitiesToSerialize;
	std::vector<Entity*> m_EntitiesToGhost;
	std::vector<LWOOBJID> m_PlayersToUpdateGhosting;
	Entity* m_ZoneControlEntity;

	uint16_t m_NetworkIdCounter;
	uint64_t m_SerializationCounter = 0;

	float m_GhostDistanceMinSqaured = 100 * 100;
	float m_GhostDistanceMaxSquared = 150 * 150;
	bool m_GhostingEnabled = true;

	std::stack<uint16_t> m_LostNetworkIds;

	// Map of spawnname to entity object ID
	std::unordered_map<std::string, LWOOBJID> m_SpawnPoints;

	// hardcore mode vars
	bool m_HardcoreMode;
	uint32_t m_HardcoreLoseUscoreOnDeathPercent;
	bool m_HardcoreDropinventoryOnDeath;
	uint32_t m_HardcoreUscoreEnemiesMultiplier;
};

#endif // ENTITYMANAGER_H
