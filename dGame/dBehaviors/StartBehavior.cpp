#include "StartBehavior.h"
#include "BehaviorBranchContext.h"

void StartBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) {
	branch.start = this->m_behaviorId;

	this->m_action->Handle(context, bit_stream, branch);
}

void StartBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) {
	branch.start = this->m_behaviorId;

	this->m_action->Calculate(context, bit_stream, branch);
}

void StartBehavior::Load() {
	this->m_action = GetAction("action");
}
