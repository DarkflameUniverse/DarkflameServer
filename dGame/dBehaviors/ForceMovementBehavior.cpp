#include "ForceMovementBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"

void ForceMovementBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	if (this->m_hitAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY && this->m_hitEnemyAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY && this->m_hitFactionAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY) {
		return;
	}

	uint32_t handle{};
	if (!bitStream->Read(handle)) {
		Game::logger->Log("ForceMovementBehavior", "Unable to read handle from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	}
	context->RegisterSyncBehavior(handle, this, branch, this->m_Duration);
}

void ForceMovementBehavior::Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	uint32_t next{};
	if (!bitStream->Read(next)) {
		Game::logger->Log("ForceMovementBehavior", "Unable to read target from bitStream, aborting Sync! %i", bitStream->GetNumberOfUnreadBits());
		return;
	}

	LWOOBJID target{};
	if (!bitStream->Read(target)) {
		Game::logger->Log("ForceMovementBehavior", "Unable to read target from bitStream, aborting Sync! %i", bitStream->GetNumberOfUnreadBits());
		return;
	}

	branch.target = target;
	auto* behavior = CreateBehavior(next);
	behavior->Handle(context, bitStream, branch);
}

void ForceMovementBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	if (this->m_hitAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY && this->m_hitEnemyAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY && this->m_hitFactionAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY) {
		return;
	}

	auto* casterEntity = Game::entityManager->GetEntity(context->caster);
	if (casterEntity != nullptr) {
		auto* controllablePhysicsComponent = casterEntity->GetComponent<ControllablePhysicsComponent>();
		if (controllablePhysicsComponent != nullptr) {

			if (m_Forward == 1) {
				controllablePhysicsComponent->SetVelocity(controllablePhysicsComponent->GetRotation().GetForwardVector() * 25);
			}

			Game::entityManager->SerializeEntity(casterEntity);
		}
	}

	const auto skillHandle = context->GetUniqueSkillId();
	bitStream->Write(skillHandle);

	context->SyncCalculation(skillHandle, this->m_Duration, this, branch);
}

void ForceMovementBehavior::Load() {
	this->m_hitAction = GetAction("hit_action");
	this->m_hitEnemyAction = GetAction("hit_action_enemy");
	this->m_hitFactionAction = GetAction("hit_action_faction");
	this->m_Duration = GetFloat("duration");
	this->m_Forward = GetFloat("forward");
	this->m_Left = GetFloat("left");
	this->m_Yaw = GetFloat("yaw");
}

void ForceMovementBehavior::SyncCalculation(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* casterEntity = Game::entityManager->GetEntity(context->caster);
	if (casterEntity != nullptr) {
		auto* controllablePhysicsComponent = casterEntity->GetComponent<ControllablePhysicsComponent>();
		if (controllablePhysicsComponent != nullptr) {

			controllablePhysicsComponent->SetPosition(controllablePhysicsComponent->GetPosition() + controllablePhysicsComponent->GetVelocity() * m_Duration);
			controllablePhysicsComponent->SetVelocity({});

			Game::entityManager->SerializeEntity(casterEntity);
		}
	}

	this->m_hitAction->Calculate(context, bitStream, branch);
	this->m_hitEnemyAction->Calculate(context, bitStream, branch);
	this->m_hitFactionAction->Calculate(context, bitStream, branch);
}
