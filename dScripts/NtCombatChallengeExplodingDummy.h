#pragma once
#include "CppScripts.h"

class NtCombatChallengeExplodingDummy : public CppScripts::Script
{
	void OnDie(Entity* self, Entity* killer) override;
	void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;
};
