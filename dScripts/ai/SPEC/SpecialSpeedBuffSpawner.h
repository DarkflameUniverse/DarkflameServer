#pragma once
#include "CppScripts.h"

class SpecialSpeedBuffSpawner : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
private:
	uint32_t m_SkillId = 500;
};
