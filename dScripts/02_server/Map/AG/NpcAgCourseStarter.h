#pragma once
#include "CppScripts.h"

class NpcAgCourseStarter : public CppScripts::Script {
	void OnStartup(Entity* self) override;

	void OnUse(Entity* self, Entity* user) override;

	void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) override;

	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
};
