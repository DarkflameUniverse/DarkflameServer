#include "SpeedBehavior.h"

#include "ControllablePhysicsComponent.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "dLogger.h"


void SpeedBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	if (m_AffectsCaster) branch.target = context->caster;

	auto* target = EntityManager::Instance()->GetEntity(branch.target);
	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (!controllablePhysicsComponent) return;

	controllablePhysicsComponent->AddSpeedboost(m_RunSpeed);
	EntityManager::Instance()->SerializeEntity(target);

	if (branch.duration > 0.0f) {
		context->RegisterTimerBehavior(this, branch);
	} else if (branch.start > 0) {
		context->RegisterEndBehavior(this, branch);
	}
}

void SpeedBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void SpeedBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	End(context, branch, LWOOBJID_EMPTY);
}

void SpeedBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	End(context, branch, second);
}

void SpeedBehavior::End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);
	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (!controllablePhysicsComponent) return;

	controllablePhysicsComponent->RemoveSpeedboost(m_RunSpeed);
	EntityManager::Instance()->SerializeEntity(target);
}

void SpeedBehavior::Load() {
	m_RunSpeed = GetFloat("run_speed");
	m_AffectsCaster = GetBoolean("affects_caster");
}
