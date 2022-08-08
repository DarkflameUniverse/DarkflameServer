#include "PullToPointBehavior.h"

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "MovementAIComponent.h"

void PullToPointBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* entity = EntityManager::Instance()->GetEntity(context->originator);

	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (entity == nullptr || target == nullptr) {
		return;
	}

	auto* movement = target->GetComponent<MovementAIComponent>();

	if (movement == nullptr) {
		return;
	}

	const auto position = branch.isProjectile ? branch.referencePosition : entity->GetPosition();

	movement->PullToPoint(position);
}

void PullToPointBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void PullToPointBehavior::Load() {
}
