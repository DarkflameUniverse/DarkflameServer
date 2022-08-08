#include "AirMovementBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"

void AirMovementBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	uint32_t handle;

	bitStream->Read(handle);

	context->RegisterSyncBehavior(handle, this, branch);
}

void AirMovementBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	const auto handle = context->GetUniqueSkillId();

	bitStream->Write(handle);
}

void AirMovementBehavior::Sync(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) {
	uint32_t behaviorId;

	bit_stream->Read(behaviorId);

	LWOOBJID target;

	bit_stream->Read(target);

	auto* behavior = CreateBehavior(behaviorId);

	if (EntityManager::Instance()->GetEntity(target) != nullptr) {
		branch.target = target;
	}

	behavior->Handle(context, bit_stream, branch);
}

void AirMovementBehavior::Load() {
}
