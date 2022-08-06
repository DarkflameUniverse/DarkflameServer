#include "ChargeUpBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "dLogger.h"

void ChargeUpBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	uint32_t handle;

	bitStream->Read(handle);

	context->RegisterSyncBehavior(handle, this, branch);
}

void ChargeUpBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
}

void ChargeUpBehavior::Sync(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	this->m_action->Handle(context, bitStream, branch);
}

void ChargeUpBehavior::Load() {
	this->m_action = GetAction("action");
}
