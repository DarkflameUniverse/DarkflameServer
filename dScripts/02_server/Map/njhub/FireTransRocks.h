#pragma once
#include "CppScripts.h"

// L_FIRE_TRANS_ROCKS.lua: a rock in the NJ monastery's fire transition, a mover between its bottom (waypoint 0) and its
// top (waypoint 1). At the top it holds 3.5 s and goes back down; at the bottom it tells the rocks' manager.
class FireTransRocks : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnArrivedAtDesiredWaypoint(Entity* self, uint32_t waypointIndex) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};
