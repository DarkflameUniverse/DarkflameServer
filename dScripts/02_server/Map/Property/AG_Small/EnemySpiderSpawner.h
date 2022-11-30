#pragma once
#include "CppScripts.h"

/*
------------------------------------------------------

-- L_ENEMY_SPIDER_SPAWNER.lua

-- Spawns a spider from the egg
-- created abeechler... 5/11/11

-- ported Max 26/07/2020

------------------------------------------------------
*/

class EnemySpiderSpawner final : public CppScripts::Script {
public:
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	void SpawnSpiderling(Entity* self);

private:
	float hatchTime = 2.0f;       //How long after full wave preparation will it take to hatch an egg ?
	float spawnTime = 2.0f;       //Once spawning begins, how long until we create Spiderlings ?
};

