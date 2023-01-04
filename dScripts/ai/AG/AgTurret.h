#pragma once
#include "CppScripts.h"

class AgTurret : public CppScripts::Script {
	void OnStartup(Entity* self);
	void OnTimerDone(Entity* self, std::string timerName);
	void OnRebuildStart(Entity* self, Entity* user);
};
