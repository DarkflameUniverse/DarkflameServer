#pragma once
#include "CppScripts.h"

class NtCombatChallengeDummy : public CppScripts::Script
{
public:
	void OnDie(Entity* self, Entity* killer) override;
	void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;
};
