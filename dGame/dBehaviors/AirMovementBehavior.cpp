#include "AirMovementBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"

void AirMovementBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	uint32_t handle{};

	if (!bitStream.Read(handle)) {
		LOG("Unable to read handle from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
		return;
	}

	context->RegisterSyncBehavior(handle, this, branch, this->m_Timeout);
}

void AirMovementBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	const auto handle = context->GetUniqueSkillId();

	bitStream.Write(handle);
}

void AirMovementBehavior::Sync(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	uint32_t behaviorId{};

	if (!bitStream.Read(behaviorId)) {
		LOG("Unable to read behaviorId from bitStream, aborting Sync! %i", bitStream.GetNumberOfUnreadBits());
		return;
	}

	// So a player can't send an arbitrary behaviorID in a modified client and cast any behavior on any air behavior
	Behavior* toSync = nullptr;
	if (m_GroundAction->GetBehaviorID() == behaviorId) {
		toSync = m_GroundAction;
	} else if (m_HitAction->GetBehaviorID() == behaviorId) {
		toSync = m_HitAction;
	} else if (m_HitActionEnemy->GetBehaviorID() == behaviorId) {
		toSync = m_HitActionEnemy;
	} else if (m_TimeoutAction->GetBehaviorID() == behaviorId) {
		toSync = m_TimeoutAction;
	} else {
		LOG("Invalid Air Movement Behavior sync for behaviorID %i on behavior %i", behaviorId, m_behaviorId);
		return;
	}

	LWOOBJID target{};

	if (!bitStream.Read(target)) {
		LOG("Unable to read target from bitStream, aborting Sync! %i", bitStream.GetNumberOfUnreadBits());
		return;
	}

	if (Game::entityManager->GetEntity(target) != nullptr) {
		branch.target = target;
	}

	toSync->Handle(context, bitStream, branch);
}

void AirMovementBehavior::Load() {
	m_Timeout = (GetFloat("timeout_ms") / 1000.0f);
	m_GroundAction = GetAction("ground_action");
	m_HitAction = GetAction("hit_action");
	m_HitActionEnemy = GetAction("hit_action_enemy");
	m_TimeoutAction = GetAction("timeout_action");
}
