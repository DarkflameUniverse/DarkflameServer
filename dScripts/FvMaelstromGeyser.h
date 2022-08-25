#pragma once
#include "CppScripts.h"

class FvMaelstromGeyser final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;


private:
	const std::string m_StartSkillTimerName = "startSkill";
	const float m_StartSkillTimerTime = 2.0;
	const std::string m_KillSelfTimerName = "killSelf";
	const float m_KillSelfTimerTime = 5.5;
	const uint32_t m_SkillID = 831;
	const uint32_t m_BehaviorID = 15500;
};
