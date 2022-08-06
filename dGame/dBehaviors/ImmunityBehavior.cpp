#include "ImmunityBehavior.h"

#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "DestroyableComponent.h"

void ImmunityBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) {
		Game::logger->Log("DamageAbsorptionBehavior", "Failed to find target (%llu)!", branch.target);

		return;
	}

	auto* destroyable = static_cast<DestroyableComponent*>(target->GetComponent(COMPONENT_TYPE_DESTROYABLE));

	if (destroyable == nullptr) {
		return;
	}

	if (!this->m_immuneBasicAttack) {
		return;
	}

	destroyable->PushImmunity();

	context->RegisterTimerBehavior(this, branch, target->GetObjectID());
}

void ImmunityBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void ImmunityBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, const LWOOBJID second) {
	auto* target = EntityManager::Instance()->GetEntity(second);

	if (target == nullptr) {
		Game::logger->Log("DamageAbsorptionBehavior", "Failed to find target (%llu)!", second);

		return;
	}

	auto* destroyable = static_cast<DestroyableComponent*>(target->GetComponent(COMPONENT_TYPE_DESTROYABLE));

	if (destroyable == nullptr) {
		return;
	}

	destroyable->PopImmunity();
}

void ImmunityBehavior::Load() {
	this->m_immuneBasicAttack = GetBoolean("immune_basic_attack");
}
