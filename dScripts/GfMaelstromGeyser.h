#pragma once
#include "CppScripts.h"

class GfMaelstromGeyser final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	const std::string m_StartSkillTimerName = "startSkill";
	const float m_StartSkillTimerTime = 2.0;
	const std::string m_KillSelfTimerName = "killSelf";
	const float m_KillSelfTimerTime = 7.5;
	const uint32_t m_SkillID = 607;
	const uint32_t m_BehaviorID = 10500;
};
