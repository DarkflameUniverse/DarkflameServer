#pragma once
#include "CppScripts.h"

class NtDirtCloudServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;

private:
	static std::map<std::string, std::vector<int32_t>> m_Missions;
};
