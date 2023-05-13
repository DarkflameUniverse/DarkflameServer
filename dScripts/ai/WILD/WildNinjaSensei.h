#pragma once
#include "CppScripts.h"

class WildNinjaSensei : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnTimerDone(Entity* self, std::string timerName);
};

