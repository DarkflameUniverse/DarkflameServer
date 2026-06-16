#pragma once
#include "Behavior.h"

class NpcCombatSkillBehavior final : public Behavior
{
public:
	std::vector<Behavior*> m_behaviors;

	float m_npcSkillTime;

	float m_maxRange{};

	/*
	 * Inherited
	 */

	explicit NpcCombatSkillBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Calculate(BehaviorContext* context, RakNet::BitStream& bit_stream, BehaviorBranchContext branch) override;

	void Load() override;
};
