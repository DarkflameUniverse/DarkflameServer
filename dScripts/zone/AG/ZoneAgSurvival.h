#pragma once
#include "CppScripts.h"
#include "BaseSurvivalServer.h"
#include <map>

class ZoneAgSurvival : public BaseSurvivalServer {
	Constants GetConstants() override;
	SpawnerNetworks GetSpawnerNetworks() override;
	MobSets GetMobSets() override;
	std::map<uint32_t, uint32_t> GetMissionsToUpdate() override;
};
