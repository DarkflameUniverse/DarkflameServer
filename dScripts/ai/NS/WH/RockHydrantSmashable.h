#pragma once
#include "CppScripts.h"

class RockHydrantSmashable : public CppScripts::Script
{
public:
	void OnDie(Entity* self, Entity* killer);
private:
	LOT ROCK_HYDRANT_BROKEN = 12293;
};

