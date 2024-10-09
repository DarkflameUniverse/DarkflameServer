#pragma once
#include "CppScripts.h"

class ElevatorSpinner : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void SpawnLegs(Entity* self);

	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;

	void TriggerDrill(Entity* self);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

    	static Entity* caster1;
};
