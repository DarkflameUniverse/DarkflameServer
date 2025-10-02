#pragma once
#include "CppScripts.h"

class ExplodingAsset : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnHit(Entity* self, Entity* attacker) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void ProgressPlayerMissions(Entity& self, Entity& player);
};
