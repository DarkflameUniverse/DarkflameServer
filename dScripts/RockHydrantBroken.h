#pragma once
#include "CppScripts.h"

class RockHydrantBroken : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};

