#pragma once
#include "CppScripts.h"

class LegoDieRoll : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnTimerDone(Entity* self, std::string timerName);
private:
	constexpr static const float animTime = 2.0f;
};

