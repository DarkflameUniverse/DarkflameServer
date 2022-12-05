
#include "ChangeIdleFlagsBehavior.h"
#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"

void ChangeIdleFlagsBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	const auto target = branch.target != LWOOBJID_EMPTY ? branch.target : context->originator;
	if (!target) return;

	GameMessages::SendChangeIdleFlags(target, m_FlagsOn, m_FlagsOff, UNASSIGNED_SYSTEM_ADDRESS);

	if (branch.duration > 0.0f) {
		context->RegisterTimerBehavior(this, branch);
	} else if (branch.start > 0) {
		context->RegisterEndBehavior(this, branch);
	}
}

void ChangeIdleFlagsBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void ChangeIdleFlagsBehavior::End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	const auto target = branch.target != LWOOBJID_EMPTY ? branch.target : context->originator;
	if (!target) return;
	// flip on and off to end behavior
	GameMessages::SendChangeIdleFlags(target, m_FlagsOff, m_FlagsOn, UNASSIGNED_SYSTEM_ADDRESS);
}

void ChangeIdleFlagsBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	End(context, branch, second);
}

void ChangeIdleFlagsBehavior::Load() {
	m_FlagsOff = static_cast<eAnimationFlags>(GetInt("flags_off", 0));
	m_FlagsOn = static_cast<eAnimationFlags>(GetInt("flags_on", 0));
}
