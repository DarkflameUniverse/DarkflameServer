#include "ClearTargetBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"


void ClearTargetBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	branch.target = LWOOBJID_EMPTY;

	this->m_action->Handle(context, bitStream, branch);
}

void ClearTargetBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	branch.target = LWOOBJID_EMPTY;

	this->m_action->Calculate(context, bitStream, branch);
}

void ClearTargetBehavior::Load() {
	this->m_action = GetAction("action");

	this->m_clearIfCaster = GetBoolean("clear_if_caster");
}
