#pragma once
#include "CppScripts.h"

class FvPassThroughWall : public CppScripts::Script
{
	/**
	 * @brief This method is called when there is a collision with self from target.
	 *
	 * @param self The Entity that called this method.
	 * @param target The Entity that self is targetting.
	 */
	void OnCollisionPhantom(Entity* self, Entity* target) override;
private:
	/**
	 * Mission ID for Friend of the Ninjas.
	 */
	int32_t friendOfTheNinjaMissionId = 848;
	/**
	 * Mission UID for Friend of the Ninjas.
	 */
	int32_t friendOfTheNinjaMissionUid = 1221;
	/**
	 * Item LOT for Maelstrom-Infused White Ninja Hood
	 */
	int32_t WhiteMaelstromHood = 2641;
	/**
	 * Item LOT for Maelstrom-Infused Black Ninja Hood
	 */
	int32_t BlackMaelstromHood = 2642;
	/**
	 * Item LOT for Red Ninja Hood - Maelstrom Infused
	 */
	int32_t RedMaelstromHood = 1889;
};
