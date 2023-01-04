#pragma once
#include "CppScripts.h"

class AgQbElevator : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	void killTimerStartup(Entity* self) const;

	// constants
	float endTime = 4.0f;
	float startTime = 8.0f;
	float killTime = 10.0f;
	float proxRadius = 5.0f;
};
