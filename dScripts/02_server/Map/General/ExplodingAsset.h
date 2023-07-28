#pragma once
#include "CppScripts.h"

class ExplodingAsset : public CppScripts::Script
{
public:
	void OnStartup(Entity* self);
	void OnHit(Entity* self, Entity* attacker);
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status);
};
