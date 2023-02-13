#pragma once
#include "ActivityManager.h"
#include "CppScripts.h"

class BaseFootRaceManager : public ActivityManager {
	void OnStartup(Entity* self) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
};
