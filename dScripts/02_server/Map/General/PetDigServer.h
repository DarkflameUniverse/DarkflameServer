#pragma once
#include "CppScripts.h"

struct DigInfo {
	LOT digLot; // The lot of the chest
	LOT spawnLot; // Option lot of pet to spawn
	int32_t requiredMission; // Optional mission required before pet can be spawned, if < 0 == don't use
	bool specificPet; // This treasure requires a specific pet to be dug up
	bool xBuild; // This treasure is retrieved from a buildable cross
	bool bouncer; // This treasure spawns a bouncer
	bool builderOnly; // Only the builder of this diggable may access the rewards, for example with crosses
};

class PetDigServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnDie(Entity* self, Entity* killer) override;

	static Entity* GetClosestTresure(NiPoint3 position);

private:
	static void ProgressPetDigMissions(const Entity* owner, const Entity* chest);
	static void SpawnPet(Entity* self, const Entity* owner, DigInfo digInfo);
	static void HandleXBuildDig(const Entity* self, Entity* owner, Entity* pet);
	static void HandleBouncerDig(const Entity* self, const Entity* owner);
	static std::vector<LWOOBJID> treasures;
	static const DigInfo defaultDigInfo;
	static const std::map<LOT, DigInfo> digInfoMap;
};
