#include "DamageReductionBehavior.h"

#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
#include "DestroyableComponent.h"

void DamageReductionBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) {
		LOG("Failed to find target (%llu)!", branch.target);

		return;
	}

	auto* destroyable = target->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}

	destroyable->SetDamageReduction(destroyable->GetDamageReduction() + m_ReductionAmount);

	if (branch.duration > 0.0f) context->RegisterTimerBehavior(this, branch, target->GetObjectID());
}

void DamageReductionBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void DamageReductionBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, const LWOOBJID second) {
	auto* target = Game::entityManager->GetEntity(second);

	if (target == nullptr) {
		LOG("Failed to find target (%llu)!", second);

		return;
	}

	auto* destroyable = target->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}
	
	auto present = static_cast<uint32_t>(destroyable->GetDamageReduction());

	auto toRemove = std::min(present, this->m_ReductionAmount);

	destroyable->SetDamageReduction(present - toRemove);
}

void DamageReductionBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) {
		Game::logger->Log("DamageReductionBehavior", "Failed to find target (%llu)!", branch.target);

		return;
	}

	auto* destroyable = target->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}
	
	auto present = static_cast<uint32_t>(destroyable->GetDamageReduction());

	auto toRemove = std::min(present, this->m_ReductionAmount);

	destroyable->SetDamageReduction(present - toRemove);
}

void DamageReductionBehavior::Load() {
	this->m_ReductionAmount = GetInt("reduction_amount");
}
