#include "DamageReductionBehavior.h"

#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "DestroyableComponent.h"

void DamageReductionBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) {
		Game::logger->Log("DamageReductionBehavior", "Failed to find target (%llu)!", branch.target);

		return;
	}

	auto* destroyable = target->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}

	destroyable->SetDamageReduction(m_ReductionAmount);

	context->RegisterTimerBehavior(this, branch, target->GetObjectID());
}

void DamageReductionBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void DamageReductionBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, const LWOOBJID second) {
	auto* target = EntityManager::Instance()->GetEntity(second);

	if (target == nullptr) {
		Game::logger->Log("DamageReductionBehavior", "Failed to find target (%llu)!", second);

		return;
	}

	auto* destroyable = target->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}

	destroyable->SetDamageReduction(0);
}

void DamageReductionBehavior::Load() {
	this->m_ReductionAmount = GetInt("reduction_amount");
}
