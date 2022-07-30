#pragma once
#include "CppScripts.h"

/**
 * Inherits regular scripts and provides functionality for objects to spawn a set of powerups over time by setting some
 * variables. Required variables should be set in the OnStartup and are:
 * - numCycles (uint32_t): the number of times to spawn powerups
 * - secPerCycle (float_t): the time between each cycle
 * - delayToFirstCycle (float_t): initial delay after startup before spawning powerups
 * - deathDelay (float_t): delay to wait before smashing after the final cycle has been initiated
 * - numberOfPowerups (uint32_t): how many powerups to spawn per cycle
 * - lootLOT (LOT): the powerup to spawn on each cycle
 */
class ScriptedPowerupSpawner : public CppScripts::Script {
public:
	/**
	 * Called by the child script after on startup
	 * \param self the object this script belongs to
	 */
	static void OnTemplateStartup(Entity* self);
	void OnTimerDone(Entity* self, std::string message) override;
};

