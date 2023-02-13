#pragma once
#include "CppScripts.h"

class GfCampfire : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;
private:
	int32_t m_skillCastId = 43;
	int32_t FIRE_COOLDOWN = 2;
};
