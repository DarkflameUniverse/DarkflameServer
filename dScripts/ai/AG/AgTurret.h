#pragma once
#include "CppScripts.h"

class AgTurret : public CppScripts::Script {
	void OnStartup(Entity* self);
	void OnTimerDone(Entity* self, std::string timerName);
	void OnQuickBuildStart(Entity* self, Entity* user);
};
