#pragma once
#include "CppScripts.h"

class AmBlueX : public CppScripts::Script {
	void OnUse(Entity* self, Entity* user) override;
	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;
private:
	const float_t m_BombTime = 3.3f;
	const uint32_t m_MissionID = 1448;
	const uint32_t m_SwordSkill = 1259;
	const uint32_t m_SwordBehavior = 29305;
	const uint32_t m_AOESkill = 1258;
	const uint32_t m_AOEBehavior = 29301;
	const LOT m_FXObject = 13808;

	// Variables
	const std::u16string m_XUsedVariable = u"XUsed";
	const std::u16string m_FlagVariable = u"flag";
	const std::u16string m_StartEffectVariable = u"startEffect";
};
