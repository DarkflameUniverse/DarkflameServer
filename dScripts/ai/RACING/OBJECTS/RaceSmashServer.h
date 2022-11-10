#pragma once
#include "CppScripts.h"

class RaceSmashServer : public CppScripts::Script {
	/**
	 * @brief When a smashable has been destroyed, this function is called.
	 *
	 * @param self The Entity that called this function.
	 * @param killer The Entity that killed this Entity.
	 */
	void OnDie(Entity* self, Entity* killer) override;
};
