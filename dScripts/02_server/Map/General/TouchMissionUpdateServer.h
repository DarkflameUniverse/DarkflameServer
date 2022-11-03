#pragma once
#include "CppScripts.h"

class TouchMissionUpdateServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
};

