#pragma once
#include "CppScripts.h"

class FallingTile : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	void OnWaypointReached(Entity* self, uint32_t waypointIndex) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};
