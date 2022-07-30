#include "SpeedBehavior.h"

#include "ControllablePhysicsComponent.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "dLogger.h"


void SpeedBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	if (m_AffectsCaster) {
		branch.target = context->caster;
	}

	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) {
		return;
	}

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();

	if (controllablePhysicsComponent == nullptr) {
		return;
	}

	const auto current = controllablePhysicsComponent->GetSpeedMultiplier();

	controllablePhysicsComponent->SetSpeedMultiplier(current + ((m_RunSpeed - 500.0f) / 500.0f));

	EntityManager::Instance()->SerializeEntity(target);

	if (branch.duration > 0.0f) {
		context->RegisterTimerBehavior(this, branch);
	} else if (branch.start > 0) {
		controllablePhysicsComponent->SetIgnoreMultipliers(true);

		context->RegisterEndBehavior(this, branch);
	}
}

void SpeedBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) {
		return;
	}

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();

	if (controllablePhysicsComponent == nullptr) {
		return;
	}

	const auto current = controllablePhysicsComponent->GetSpeedMultiplier();

	controllablePhysicsComponent->SetSpeedMultiplier(current - ((m_RunSpeed - 500.0f) / 500.0f));

	EntityManager::Instance()->SerializeEntity(target);
}

void SpeedBehavior::End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) {
		return;
	}

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();

	if (controllablePhysicsComponent == nullptr) {
		return;
	}

	const auto current = controllablePhysicsComponent->GetSpeedMultiplier();

	controllablePhysicsComponent->SetIgnoreMultipliers(false);

	controllablePhysicsComponent->SetSpeedMultiplier(current - ((m_RunSpeed - 500.0f) / 500.0f));

	EntityManager::Instance()->SerializeEntity(target);
}

void SpeedBehavior::Load() {
	m_RunSpeed = GetFloat("run_speed");

	if (m_RunSpeed < 500.0f) {
		m_RunSpeed = 500.0f;
	}

	m_AffectsCaster = GetBoolean("affects_caster");
}
