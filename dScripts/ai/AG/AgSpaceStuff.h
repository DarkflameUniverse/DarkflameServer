#pragma once
#include "CppScripts.h"

class AgSpaceStuff : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnTimerDone(Entity* self, std::string timerName);
};

