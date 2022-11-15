#pragma once
#include "CppScripts.h"

class MonCoreNookDoors : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;

	void SpawnDoor(Entity* self);

	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;

	void OnTimerDone(Entity* self, std::string timerName) override;
};

