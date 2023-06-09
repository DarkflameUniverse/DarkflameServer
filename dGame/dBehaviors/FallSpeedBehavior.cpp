#include "FallSpeedBehavior.h"

#include "ControllablePhysicsComponent.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"


void FallSpeedBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	// make sure required parameter has non-default value
	if (m_PercentSlowed == 0.0f) return;
	auto* target = EntityManager::Instance()->GetEntity(branch.target);
	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (!controllablePhysicsComponent) return;
	controllablePhysicsComponent->SetGravityScale(m_PercentSlowed);
	EntityManager::Instance()->SerializeEntity(target);

	if (branch.duration > 0.0f) {
		context->RegisterTimerBehavior(this, branch);
	} else if (branch.start > 0) {
		context->RegisterEndBehavior(this, branch);
	}
}

void FallSpeedBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void FallSpeedBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	End(context, branch, second);
}

void FallSpeedBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	End(context, branch, LWOOBJID_EMPTY);
}

void FallSpeedBehavior::End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);
	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (!controllablePhysicsComponent) return;
	controllablePhysicsComponent->SetGravityScale(1);
	EntityManager::Instance()->SerializeEntity(target);
}

void FallSpeedBehavior::Load(){
	m_PercentSlowed = GetFloat("percent_slowed");
}
