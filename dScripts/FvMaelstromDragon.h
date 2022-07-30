#pragma once
#include "CppScripts.h"

class FvMaelstromDragon : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnDie(Entity* self, Entity* killer) override;
	void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
};
