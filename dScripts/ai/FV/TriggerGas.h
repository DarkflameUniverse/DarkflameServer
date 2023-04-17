#pragma once
#include "CppScripts.h"

class TriggerGas : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	void OnOffCollisionPhantom(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	std::string m_TimerName = "gasTriggerDamage";
	float m_Time = 3.0f;
	uint32_t m_MaelstromHelmet = 3068;
	uint32_t m_FogDamageSkill = 103;
};
