#pragma once
#include "CppScripts.h"

class ImaginationBackpackHealServer : public CppScripts::Script {
	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;
};
