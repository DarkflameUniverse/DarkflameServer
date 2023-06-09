#include "DamageAbsorptionBehavior.h"

#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "DestroyableComponent.h"

void DamageAbsorptionBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) {
		Game::logger->Log("DamageAbsorptionBehavior", "Failed to find target (%llu)!", branch.target);

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
	auto* target = EntityManager::Instance()->GetEntity(second);

	if (target == nullptr) {
		Game::logger->Log("DamageAbsorptionBehavior", "Failed to find target (%llu)!", second);

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
