#pragma once
#include "CppScripts.h"

class BootyDigServer : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnPlayerLoaded(Entity* self, Entity* player) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
};
