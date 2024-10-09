#pragma once
#include "CppScripts.h"

class FtMovingDoor4Statues : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;

	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;

	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	int Progress;

    	static Entity* player1;
    	static Entity* player2;
    	static Entity* player3;
    	static Entity* player4;

};

