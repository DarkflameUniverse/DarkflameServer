#pragma once
#include "CppScripts.h"

class AmSkullkinDrillStand : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) override;

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
};
