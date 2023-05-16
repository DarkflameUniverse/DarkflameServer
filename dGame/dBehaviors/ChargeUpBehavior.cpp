#include "ChargeUpBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "Game.h"
#include "Logger.h"

void ChargeUpBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	uint32_t handle{};

	if (!bitStream->Read(handle)) {
		Log("Unable to read handle from bitStream, aborting Handle! variable_type");
		return;
	};

	context->RegisterSyncBehavior(handle, this, branch, this->m_MaxDuration);
}

void ChargeUpBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
}

void ChargeUpBehavior::Sync(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	this->m_action->Handle(context, bitStream, branch);
}

void ChargeUpBehavior::Load() {
	this->m_action = GetAction("action");
	this->m_MaxDuration = GetFloat("max_duration");
}
