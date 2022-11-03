#pragma once
#include "CppScripts.h"

class RaceMaelstromGeiser : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
};
