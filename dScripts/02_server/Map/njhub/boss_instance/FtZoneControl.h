#pragma once
#include "CppScripts.h"

class FtZoneControl : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnZoneLoadedInfo(Entity* self);

	void OnPlayerLoaded(Entity* self, Entity* player) override;

	void OnPlayerExit(Entity* self, Entity* player) override;

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

    static Entity* player1;
    static Entity* player2;
    static Entity* player3;
    static Entity* player4;

	int PlayerCount;

	int BossPlayerCount;

	int TeamChecked;

	int PlayersInTeam;


	int Player1Loaded = 0;
	int Player2Loaded = 0;
	int Player3Loaded = 0;
	int Player4Loaded = 0;

};
