#pragma once
#include "CppScripts.h"

class TriggerGas : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	void OnOffCollisionPhantom(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	std::string m_timerName = "gasTriggerDamage";
	float m_time = 3.0f;
	uint32_t m_maelstromHelmet = 3068;
	uint32_t m_fogDamageSkill = 103;
};
