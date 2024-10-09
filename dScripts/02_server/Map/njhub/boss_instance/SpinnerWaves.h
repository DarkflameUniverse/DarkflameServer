#pragma once
#include "CppScripts.h"

class SpinnerWaves : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) override;

	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;

	void TriggerDrill(Entity* self);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

    	static Entity* caster1;
};
