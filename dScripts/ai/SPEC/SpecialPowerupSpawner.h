#pragma once
#include "CppScripts.h"

class SpecialPowerupSpawner : public CppScripts::Script {
public:
	SpecialPowerupSpawner(uint32_t skillId) {
		m_SkillId = skillId;
	};
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
private:
	uint32_t m_SkillId = 0;
};
