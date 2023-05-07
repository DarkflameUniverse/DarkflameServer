#include "BlockBehavior.h"

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "DestroyableComponent.h"

void BlockBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	const auto target = context->originator;

	auto* entity = EntityManager::Instance()->GetEntity(target);

	if (entity == nullptr) {
		Game::logger->Log("DamageAbsorptionBehavior", "Failed to find target (%llu)!", branch.target);

		return;
	}

	auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

	if (destroyableComponent == nullptr) {
		return;
	}

	destroyableComponent->SetAttacksToBlock(this->m_numAttacksCanBlock);

	if (branch.start > 0) {
		context->RegisterEndBehavior(this, branch);
	} else if (branch.duration > 0) {
		context->RegisterTimerBehavior(this, branch);
	}
}

void BlockBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void BlockBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	const auto target = context->originator;

	auto* entity = EntityManager::Instance()->GetEntity(target);

	if (entity == nullptr) {
		Game::logger->Log("DamageAbsorptionBehavior", "Failed to find target (%llu)!", branch.target);

		return;
	}

	auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

	destroyableComponent->SetAttacksToBlock(this->m_numAttacksCanBlock);

	if (destroyableComponent == nullptr) {
		return;
	}

	destroyableComponent->SetAttacksToBlock(0);
}

void BlockBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	UnCast(context, branch);
}

void BlockBehavior::End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	UnCast(context, branch);
}

void BlockBehavior::Load() {
	this->m_numAttacksCanBlock = GetInt("num_attacks_can_block");
}
