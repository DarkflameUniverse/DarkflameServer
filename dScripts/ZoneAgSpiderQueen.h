#pragma once
#include "ActivityManager.h"
#include "ZoneAgProperty.h"

class ZoneAgSpiderQueen : ZoneAgProperty, ActivityManager {
public:
	void OnStartup(Entity* self) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnPlayerExit(Entity* self, Entity* player) override;
	void BasePlayerLoaded(Entity* self, Entity* player) override;
	void SetGameVariables(Entity* self) override;
protected:
	std::string destroyedCinematic = "DesMaelstromInstance";
	const LOT chestObject = 16318;
};
