#include "TauntBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "BaseCombatAIComponent.h"
#include "EntityManager.h"
#include "Logger.h"


void TauntBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) {
		Log::Warn("Failed to find target ({})!", branch.target);

		return;
	}

	auto* combatComponent = target->GetComponent<BaseCombatAIComponent>();

	if (combatComponent != nullptr) {
		combatComponent->Taunt(context->originator, m_threatToAdd);
	}
}

void TauntBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) {
		Log::Warn("Failed to find target ({})!", branch.target);

		return;
	}

	auto* combatComponent = target->GetComponent<BaseCombatAIComponent>();

	if (combatComponent != nullptr) {
		combatComponent->Taunt(context->originator, m_threatToAdd);
	}
}

void TauntBehavior::Load() {
	this->m_threatToAdd = GetFloat("threat to add");
}

