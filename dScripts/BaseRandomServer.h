#pragma once

#include <map>

class Spawner;
class BaseRandomServer
{
public:
	struct ZoneEntry
	{
		LOT lot;
		int32_t num;
		std::string name;
	};

	struct Zone
	{
		std::vector<BaseRandomServer::ZoneEntry> entries;
		int32_t iChance;
	};

	void BaseStartup(Entity* self);
	void CheckEvents(Entity* self);
	void SpawnMapZones(Entity* self);
	void SpawnSection(Entity* self, const std::string& sectionName, float iMultiplier);
	void SetSpawnerNetwork(Entity* self, const std::string& spawnerName, int32_t spawnNum, LOT spawnLOT);
	BaseRandomServer::Zone* GetRandomLoad(Entity* self, const std::string& sectionName);
	void SpawnersUp(Entity* self);
	void SpawnersDown(Entity* self);
	void BaseOnTimerDone(Entity* self, const std::string& timerName);

	void NotifySpawnerOfDeath(Entity* self, Spawner* spawner);
	void NamedEnemyDeath(Entity* self, Spawner* spawner);

	void SpawnNamedEnemy(Entity* self);

	void NamedTimerDone(Entity* self, const std::string& timerName);

protected:
	std::vector<int32_t> namedMobs = {
		11988, // Ronin
		11984, // Spiderling
		12654, // Horsemen
		11986, // Admiral
		11983, // Mech
		11982, // Stromling
		11985  // Pirate
	};
	std::map<std::string, float> sectionMultipliers = {};
	int32_t mobDeathResetNumber = 30;
	std::string zonePrefix = "em";
	int32_t zoneNameConst = 2;
	int32_t sectionIDConst = 3;
	std::string zoneName = "fin";
	std::vector<Zone> zones = {};
	int32_t changeNum = 15;
	int32_t respawnTime = 80;
	std::vector<Spawner*> spawnersWatched;
};
