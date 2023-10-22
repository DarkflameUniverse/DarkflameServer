#include "DamageAbsorptionBehavior.h"

#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
#include "DestroyableComponent.h"

void DamageAbsorptionBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) {
		LOG("Failed to find target (%llu)!", branch.target);

		return;
	}

	auto* destroyable = target->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}

	destroyable->SetDamageToAbsorb(static_cast<uint32_t>(destroyable->GetDamageToAbsorb()) + this->m_absorbAmount);

	destroyable->SetIsShielded(true);

	context->RegisterTimerBehavior(this, branch, target->GetObjectID());
}

void DamageAbsorptionBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void DamageAbsorptionBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, const LWOOBJID second) {
	auto* target = Game::entityManager->GetEntity(second);

	if (target == nullptr) {
		LOG("Failed to find target (%llu)!", second);

		return;
	}

	auto* destroyable = target->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}

	const auto present = static_cast<uint32_t>(destroyable->GetDamageToAbsorb());

	const auto toRemove = std::min(present, this->m_absorbAmount);

	destroyable->SetDamageToAbsorb(present - toRemove);
}

void DamageAbsorptionBehavior::Load() {
	this->m_absorbAmount = GetInt("absorb_amount");
}
