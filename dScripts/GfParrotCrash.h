#pragma once
#include "CppScripts.h"

class GfParrotCrash : public CppScripts::Script {
public:
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
private:
	const uint32_t m_SlowSkillID = 795;
	const uint32_t m_SlowBehaviorID = 14214;
	const uint32_t m_UnslowSkillID = 796;
	const uint32_t m_UnslowBehaviorID = 14215;
};

