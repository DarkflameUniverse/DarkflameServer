#pragma once
#include "CppScripts.h"

class AmBridge : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnQuickBuildComplete(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};
