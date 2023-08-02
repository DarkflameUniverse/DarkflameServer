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
	
	std::string ss = "behavior ";

	int i = 1;

	while (true) {
		std::string s = ss + std::to_string(i);

		if (GetInt(s, 0) == 0) {
			break;
		}

		m_behaviors.push_back(GetAction(s));

		++i;
	}
}
