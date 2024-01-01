# pragma once
#include "CppScripts.h"

class GfApeSmashingQB : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnQuickBuildComplete(Entity* self, Entity* target) override;
};
