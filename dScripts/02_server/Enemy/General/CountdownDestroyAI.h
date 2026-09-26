#pragma once
#include "CppScripts.h"

#include "GameMessages.h"

class CountdownDestroyAI : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void CountdownStartup(Entity& self);
	void OnHit(Entity* self, Entity* attacker) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	bool OnNotifyCombatAIStateChange(Entity& self, GameMessages::NotifyCombatAIStateChange& msg);
};
