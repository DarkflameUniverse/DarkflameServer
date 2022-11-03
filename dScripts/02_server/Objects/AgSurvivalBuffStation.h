#pragma once
#include "CppScripts.h"

class AgSurvivalBuffStation : public CppScripts::Script
{
public:
	/**
	 * @brief When the rebuild of self is complete, we calculate the behavior that is assigned to self in the database.
	 *
	 * @param self The Entity that called this script.
	 * @param target The target of the self that called this script.
	 */
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	/**
	 * Skill ID for the buff station.
	 */
	uint32_t skillIdForBuffStation = 201;
	/**
	 * Behavior ID for the buff station.
	 */
	uint32_t behaviorIdForBuffStation = 1784;
	/**
	 * Timer for dropping armor.
	 */
	float dropArmorTimer = 6.0f;
	/**
	 * Timer for dropping life.
	 */
	float dropLifeTimer = 3.0f;
	/**
	 * Timer for dropping imagination.
	 */
	float dropImaginationTimer = 4.0f;
	/**
	 * Timer for smashing.
	 */
	float smashTimer = 25.0f;
	/**
	 * LOT for armor powerup.
	 */
	LOT armorPowerup = 6431;
	/**
	 * LOT for life powerup.
	 */
	LOT lifePowerup = 177;
	/**
	 * LOT for imagination powerup.
	 */
	LOT imaginationPowerup = 935;
};
