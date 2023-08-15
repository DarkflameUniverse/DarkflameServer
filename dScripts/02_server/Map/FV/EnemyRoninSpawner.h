#pragma once
#include "CppScripts.h"

class EnemyRoninSpawner final : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnHit(Entity* self, Entity* attacker) override;
	void StartHatching(Entity* self);
};

