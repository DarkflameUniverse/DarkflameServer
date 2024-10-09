#pragma once
#include "CppScripts.h"

class FtLeg : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;

	void TriggerDrill(Entity* self);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

	std::vector<int32_t> MissionIds = { 2206, 2207, 2208 };
};
