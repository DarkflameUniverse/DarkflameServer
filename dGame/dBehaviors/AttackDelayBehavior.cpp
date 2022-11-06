#include "AttackDelayBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "Game.h"
#include "dLogger.h"

void AttackDelayBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	uint32_t handle;

	bitStream->Read(handle);

	for (auto i = 0u; i < this->m_numIntervals; ++i) {
		context->RegisterSyncBehavior(handle, this, branch);
	}
}

void AttackDelayBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	const auto handle = context->GetUniqueSkillId();

	bitStream->Write(handle);

	context->foundTarget = true;

	for (auto i = 0u; i < this->m_numIntervals; ++i) {
		const auto multiple = static_cast<float>(i + 1);

		context->SyncCalculation(handle, this->m_delay * multiple, this, branch, m_ignoreInterrupts);
	}
}

void AttackDelayBehavior::Sync(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	this->m_action->Handle(context, bitStream, branch);
}

void AttackDelayBehavior::SyncCalculation(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	PlayFx(u"cast", context->originator);

	this->m_action->Calculate(context, bitStream, branch);
}

void AttackDelayBehavior::Load() {
	this->m_numIntervals = GetInt("num_intervals");

	this->m_action = GetAction("action");

	this->m_delay = GetFloat("delay");

	this->m_ignoreInterrupts = GetBoolean("ignore_interrupts");

	if (this->m_numIntervals == 0) {
		this->m_numIntervals = 1;
	}
}
