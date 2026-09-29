#pragma once
#include "CppScripts.h"

// TURRET.lua: the Assembly Engineer turret. Its combat AI is off until it is built; it can't be stunned, interrupted,
// knocked back or pulled; it dies 30 seconds after it was placed, once nobody is building it.
class Turret : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnQuickBuildStart(Entity* self, Entity* target) override;
	void OnQuickBuildNotifyState(Entity* self, eQuickBuildState state) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	static void SetCombatAI(Entity* self, bool enabled);
	static constexpr int32_t KILL_TIME = 30;
};
