#pragma once
#include "CppScripts.h"

class FvMaelstromGeyser final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;


private:
	std::string m_StartSkillTimerName = "startSkill";
	float m_StartSkillTimerTime = 2.0;
	std::string m_KillSelfTimerName = "killSelf";
	float m_KillSelfTimerTime = 5.5;
	uint32_t m_SkillID = 831;
	uint32_t m_BehaviorID = 15500;
};
