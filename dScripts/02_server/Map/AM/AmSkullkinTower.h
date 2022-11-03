#pragma once
#include "CppScripts.h"

class AmSkullkinTower : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void SpawnLegs(Entity* self, const std::string& loc);

	void OnChildLoaded(Entity* self, Entity* child);

	void NotifyDie(Entity* self, Entity* other, Entity* killer);

	void OnChildRemoved(Entity* self, Entity* child);

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;

	void OnTimerDone(Entity* self, std::string timerName) override;
};
