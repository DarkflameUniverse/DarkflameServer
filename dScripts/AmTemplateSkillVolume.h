#pragma once
#include "CppScripts.h"

class AmTemplateSkillVolume : public CppScripts::Script
{
public:
	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;
};
