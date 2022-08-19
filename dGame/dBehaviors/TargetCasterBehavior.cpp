#include "TargetCasterBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"


void TargetCasterBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) {
	branch.target = context->caster;

	this->m_action->Handle(context, bit_stream, branch);
}

void TargetCasterBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	this->m_action->UnCast(context, branch);
}

void TargetCasterBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) {
	branch.target = context->caster;

	this->m_action->Calculate(context, bit_stream, branch);
}

void TargetCasterBehavior::Load() {
	this->m_action = GetAction("action");
}

