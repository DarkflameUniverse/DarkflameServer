#pragma once
#include "CppScripts.h"

class SpecialImaginePowerupSpawner final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
};
