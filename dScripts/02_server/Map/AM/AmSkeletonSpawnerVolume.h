#pragma once
#include "CppScripts.h"

// A volume over the Skullkin area of Crux Prime that spawns more Skullkin while many players are in it.
class AmSkeletonSpawnerVolume : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	void OnOffCollisionPhantom(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	// Sets the spawner networks to the higher or lower counts.
	static void SetSpawnCounts(Entity* self, bool max);

	// Players needed in the volume for the higher counts.
	static constexpr int32_t ChangeNum = 10;

	// Seconds after a change before the counts may change again.
	static constexpr float ChangeCooldown = 60.0f;
};
