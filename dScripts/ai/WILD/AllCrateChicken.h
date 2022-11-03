#pragma once
#include "CppScripts.h"

class AllCrateChicken : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnTimerDone(Entity* self, std::string timerName);
};

