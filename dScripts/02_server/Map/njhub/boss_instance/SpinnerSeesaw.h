#pragma once
#include "CppScripts.h"

class SpinnerSeesaw : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void SpawnLegs(Entity* self);

	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;

	void SpinnerAscend(Entity* self);

	void SpinnerDescend(Entity* self);

	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) override;

	void OnTimerDone(Entity* self, std::string timerName) override;
};
