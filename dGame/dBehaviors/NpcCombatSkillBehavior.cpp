#include "NpcCombatSkillBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"


void NpcCombatSkillBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) {
	context->skillTime = this->m_npcSkillTime;

	for (auto* behavior : this->m_behaviors) {
		behavior->Calculate(context, bit_stream, branch);
	}
}

void NpcCombatSkillBehavior::Load() {
	this->m_npcSkillTime = GetFloat("npc skill time");

	const auto parameters = GetParameterNames();

	for (const auto& parameter : parameters) {
		if (parameter.first.rfind("behavior", 0) == 0) {
			auto* action = GetAction(parameter.second);

			this->m_behaviors.push_back(action);
		}
	}
}
