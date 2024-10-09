#pragma once
#include "CppScripts.h"

class TiersKillcountEvent : public CppScripts::Script
{
public:

	void OnStartup(Entity* self) override;

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;

	void SpawnLegs(Entity* self, const std::string& loc);

	void OnChildLoaded(Entity* self, Entity* child);

	void NotifyDie(Entity* self, Entity* other, Entity* killer);

	void OnChildRemoved(Entity* self, Entity* child);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

    static Entity* player1;
    static Entity* player2;
    static Entity* player3;
    static Entity* player4;

};
