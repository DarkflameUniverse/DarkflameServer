#include "DurationBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"

void DurationBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	branch.duration = this->m_duration;

	this->m_action->Handle(context, bitStream, branch);
}

void DurationBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	branch.duration = this->m_duration;

	this->m_action->Calculate(context, bitStream, branch);
}

void DurationBehavior::Load() {
	this->m_duration = GetFloat("duration");

	this->m_action = GetAction("action");
}
