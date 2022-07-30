#pragma once
#include "CppScripts.h"

class FvFlyingCreviceDragon : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnArrived(Entity* self);
};
