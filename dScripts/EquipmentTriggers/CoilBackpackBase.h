#ifndef __GemPackBase__H__
#define __GemPackBase__H__

#include "CppScripts.h"

class CoilBackpackBase: public CppScripts::Script {
public:
	CoilBackpackBase(uint32_t skillId) {
		m_SkillId = skillId;
	};

	void OnFactionTriggerItemEquipped(Entity* itemOwner, LWOOBJID itemObjId) override;
	void NotifyHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;
	void OnFactionTriggerItemUnequipped(Entity* itemOwner, LWOOBJID itemObjId) override;
private:
	uint32_t m_SkillId = 0;
};

#endif  //!__GemPackBase__H__
