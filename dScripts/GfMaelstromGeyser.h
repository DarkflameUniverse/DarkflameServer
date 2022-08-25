#pragma once
#include "CppScripts.h"

class GfMaelstromGeyser final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	std::string m_StartSkillTimerName = "startSkill";
	float m_StartSkillTimerTime = 2.0;
	std::string m_KillSelfTimerName = "killSelf";
	float m_KillSelfTimerTime = 7.5;
	uint32_t m_SkillID = 607;
	uint32_t m_BehaviorID = 10500;
};
