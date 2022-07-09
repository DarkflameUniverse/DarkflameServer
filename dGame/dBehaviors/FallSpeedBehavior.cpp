#include "FallSpeedBehavior.h"

#include "ControllablePhysicsComponent.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"


void FallSpeedBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	branch.target = context->caster;

	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();

	if (!controllablePhysicsComponent) return;

	const auto current = controllablePhysicsComponent->GetGravityScale();

	controllablePhysicsComponent->SetGravityScale(m_PercentSlowed);

	EntityManager::Instance()->SerializeEntity(target);

	if (branch.duration > 0.0f) {
		context->RegisterTimerBehavior(this, branch);
	} else if (branch.start > 0) {
		controllablePhysicsComponent->SetIgnoreMultipliers(true);

		context->RegisterEndBehavior(this, branch);
	}
}

void FallSpeedBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();

	if (!controllablePhysicsComponent) return;

	const auto current = controllablePhysicsComponent->GetGravityScale();

	controllablePhysicsComponent->SetGravityScale(m_PercentSlowed);

	EntityManager::Instance()->SerializeEntity(target);
}

void FallSpeedBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();

	if (!controllablePhysicsComponent) return;

	// const auto current = controllablePhysicsComponent->GetGravityScale();

	controllablePhysicsComponent->SetIgnoreMultipliers(false);

	controllablePhysicsComponent->SetGravityScale(1);

	EntityManager::Instance()->SerializeEntity(target);
}

void FallSpeedBehavior::End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (!target) return;

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();

	if (!controllablePhysicsComponent) return;

	// const auto current = controllablePhysicsComponent->GetGravityScale();

	controllablePhysicsComponent->SetIgnoreMultipliers(false);

	controllablePhysicsComponent->SetGravityScale(1);

	EntityManager::Instance()->SerializeEntity(target);
}

void FallSpeedBehavior::Load(){
	m_PercentSlowed = GetFloat("percent_slowed");
}
