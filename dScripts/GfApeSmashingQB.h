# pragma once
#include "CppScripts.h"

class GfApeSmashingQB : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnRebuildComplete(Entity* self, Entity* target) override;
};
