#pragma once
#include "CppScripts.h"

class FtGarmadonTornado : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) override;

	void OnWaypointReached(Entity* self, uint32_t waypointIndex) override;

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;

	void OnCollisionPhantom(Entity* self, Entity* target) override;

	void OnTimerDone(Entity* self, std::string timerName) override;


private:

	LWOOBJID selectedPlayerID = LWOOBJID_EMPTY;

	static Entity* randomPlayer;

	std::string pathString = "BossTornadoPath_";

    	int bombLOT = 16891;

	int currentWaypoint;

	int randomNum;

    	std::string gameSpaceVolume = "BossGameSpace";

	uint32_t TornadoHitSkill = 1741;

};