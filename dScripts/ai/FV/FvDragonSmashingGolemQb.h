#pragma once
#include "CppScripts.h"

class FvDragonSmashingGolemQb : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnRebuildNotifyState(Entity* self, eRebuildState state) override;
};
