#pragma once
#include "CppScripts.h"

class StinkyFishTarget : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};
