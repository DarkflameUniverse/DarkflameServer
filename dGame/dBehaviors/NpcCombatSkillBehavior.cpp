#include "NpcCombatSkillBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"


void NpcCombatSkillBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bit_stream, BehaviorBranchContext branch) {
	context->skillTime = this->m_npcSkillTime;
	const auto* const targetEntity = Game::entityManager->GetEntity(branch.target);
	const auto* const sourceEntity = Game::entityManager->GetEntity(context->caster);

	bool cast = true;
	// Check that the target is within the cast range
	if (targetEntity && sourceEntity && this->m_maxRange != 0.0f) {
		const auto targetPos = targetEntity->GetPosition();
		const auto sourcePos = sourceEntity->GetPosition();
		const auto distance = NiPoint3::DistanceSquared(targetPos, sourcePos);
		cast = distance >= this->m_minRange && distance <= this->m_maxRange;
	}

	if (cast) {
		for (auto* behavior : this->m_behaviors) {
			behavior->Calculate(context, bit_stream, branch);
		}
	} else {
		// We failed to find a valid target, do not continue the behavior
		context->foundTarget = false;
	}
}

void NpcCombatSkillBehavior::Load() {
	this->m_npcSkillTime = GetFloat("npc skill time");
	this->m_minRange = GetFloat("min range") * 0.9f; // Make the min and max 10% smaller to account for server/client position disagreements
	this->m_minRange *= this->m_minRange;
	this->m_maxRange = GetFloat("max range") * 0.9f; // Make the min and max 10% smaller to account for server/client position disagreements
	this->m_maxRange *= this->m_maxRange;

	const auto parameters = GetParameterNames();

	for (const auto& [parameter, value] : parameters) {
		if (parameter.rfind("behavior", 0) == 0) {
			auto* action = GetAction(value);

			this->m_behaviors.push_back(action);
		}
	}
}
