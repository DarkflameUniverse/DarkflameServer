#include "FallSpeedBehavior.h"

#include "ControllablePhysicsComponent.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"


void FallSpeedBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	// make sure required parameter has non-default value
	if (m_PercentSlowed == 0.0f) return;
	auto* target = Game::entityManager->GetEntity(branch.target);
	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (!controllablePhysicsComponent) return;
	controllablePhysicsComponent->SetGravityScale(m_PercentSlowed);
	Game::entityManager->SerializeEntity(target);

	if (branch.duration > 0.0f) {
		context->RegisterTimerBehavior(this, branch);
	} else if (branch.start > 0) {
		context->RegisterEndBehavior(this, branch);
	}
}

void FallSpeedBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void FallSpeedBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	End(context, branch, second);
}

void FallSpeedBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	End(context, branch, LWOOBJID_EMPTY);
}

void FallSpeedBehavior::End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	auto* target = Game::entityManager->GetEntity(branch.target);
	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (!controllablePhysicsComponent) return;
	controllablePhysicsComponent->SetGravityScale(1);
	Game::entityManager->SerializeEntity(target);
}

void FallSpeedBehavior::Load(){
	m_PercentSlowed = GetFloat("percent_slowed");
}
