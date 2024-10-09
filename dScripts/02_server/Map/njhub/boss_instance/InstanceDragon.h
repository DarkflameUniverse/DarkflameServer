#pragma once
#include "CppScripts.h"
#include "RenderComponent.h"

class InstanceDragon : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnUse(Entity* self, Entity* user) override;

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;

	void OnTimerDone(Entity* self, std::string timerName) override;

};
