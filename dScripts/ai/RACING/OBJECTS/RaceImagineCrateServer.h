#pragma once
#include "CppScripts.h"

class RaceImagineCrateServer : public CppScripts::Script
{
public:
	/**
	 * @brief When a boost smashable has been smashed, this function is called
	 *
	 * @param self The Entity that called this function.
	 * @param killer The Entity that killed this Entity.
	 */
	void OnDie(Entity* self, Entity* killer) override;
};
