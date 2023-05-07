#pragma once
#include "CppScripts.h"

class ActMine : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnRebuildNotifyState(Entity* self, eRebuildState state) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	int MAX_WARNINGS = 3;
	float MINE_RADIUS = 10.0;
	float TICK_TIME = 0.25;
	float BLOWED_UP_TIME = 0.1;
	uint32_t SKILL_ID = 317;
	uint32_t BEHAVIOR_ID = 3719;
};

