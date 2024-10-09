#pragma once
#include "CppScripts.h"

class SpawnerSpinner30 : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;

	void SpawnLegs(Entity* self, const std::string& loc);

	void OnChildLoaded(Entity* self, Entity* child);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

	int AIsActivated;
	int BIsActivated;
	int CIsActivated;
	int DIsActivated;

    static Entity* statue;
};
