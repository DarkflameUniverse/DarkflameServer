#include "AirMovementBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"

void AirMovementBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	uint32_t handle{};

	if (!bitStream->Read(handle)) {
		Log("Unable to read handle from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	}

	context->RegisterSyncBehavior(handle, this, branch, this->m_Timeout);
}

void AirMovementBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	const auto handle = context->GetUniqueSkillId();

	bitStream->Write(handle);
}

void AirMovementBehavior::Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	uint32_t behaviorId{};

	if (!bitStream->Read(behaviorId)) {
		Log("Unable to read behaviorId from bitStream, aborting Sync! %i", bitStream->GetNumberOfUnreadBits());
		return;
	}

	LWOOBJID target{};

	if (!bitStream->Read(target)) {
		Log("Unable to read target from bitStream, aborting Sync! %i", bitStream->GetNumberOfUnreadBits());
		return;
	}

	auto* behavior = CreateBehavior(behaviorId);

	if (EntityManager::Instance()->GetEntity(target) != nullptr) {
		branch.target = target;
	}

	behavior->Handle(context, bitStream, branch);
}

void AirMovementBehavior::Load() {
	this->m_Timeout = (GetFloat("timeout_ms") / 1000.0f);
}
