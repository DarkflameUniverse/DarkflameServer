#pragma once
#include "CppScripts.h"

class AmDarklingDragon : public CppScripts::Script
{
public:
	/**
	 * @brief When called, this function will make self immune to stuns and initialize a weakspot boolean to false.
	 *
	 * @param self The Entity that called this function.
	 */
	void OnStartup(Entity* self) override;
	/**
	 * @brief When called, this function will destroy the golem if it was alive, otherwise returns immediately.
	 *
	 * @param self The Entity that called this function.
	 * @param killer The Entity that killed self.
	 */
	void OnDie(Entity* self, Entity* killer) override;
	/**
	 * @brief When self is hit or healed, this function will check if self is at zero armor.  If self is at zero armor, a golem Entity Quick Build
	 * is spawned that, when built, will reveal a weakpoint on the dragon that if hit will smash the dragon instantly.  If at more than zero armor,
	 * this function returns early.
	 *
	 * @param self The Entity that was hit.
	 * @param attacker The Entity that attacked self.
	 * @param damage The amount of damage attacker did to self.
	 */
	void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;
	/**
	 * @brief Called when self has a timer that ended.
	 *
	 * @param self The Entity who owns a timer that finished.
	 * @param timerName The name of a timer attacked to self that has ended.
	 */
	void OnTimerDone(Entity* self, std::string timerName) override;
	/**
	 * @brief When the Client has finished rebuilding the Golem for the dragon, this function exposes the weak spot for a set amount of time.
	 *
	 * @param self The Entity that called this script.
	 * @param sender The Entity that sent a fired event.
	 * @param args The argument that tells us what event has been fired off.
	 * @param param1 Unused in this script.
	 * @param param2 Unused in this script.
	 * @param param3 Unused in this script.
	 */
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
};
