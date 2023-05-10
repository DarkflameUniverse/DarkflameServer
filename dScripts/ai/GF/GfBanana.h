#pragma once
#include "CppScripts.h"

class GfBanana final : public CppScripts::Script
{
public:
	void SpawnBanana(Entity* self);

	void OnStartup(Entity* self) override;

	void OnHit(Entity* self, Entity* attacker) override;

	void OnTimerDone(Entity* self, std::string timerName) override;
};
