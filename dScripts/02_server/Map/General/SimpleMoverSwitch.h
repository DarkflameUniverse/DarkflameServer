#pragma once
#include "CppScripts.h"

// L_SIMPLE_MOVER_SWITCH.lua with L_BASE_SWITCH_ACTIVATED_SERVER.lua: a moving platform that a switch sends to and fro
// (the NJ monastery's ice and lightning attic puzzles). It asks the switches in its switchGroup to tell it when they
// are activated and deactivated, and goes to waypoint 1 on the one and back to waypoint 0 on the other.
class SimpleMoverSwitch : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	void RequestSwitches(Entity* self);
};
