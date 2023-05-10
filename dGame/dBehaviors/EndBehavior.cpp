#include "EndBehavior.h"

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"

void EndBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	context->InvokeEnd(this->m_startBehavior);
}

void EndBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	context->InvokeEnd(this->m_startBehavior);
}

void EndBehavior::Load() {
	this->m_startBehavior = GetInt("start_action");
}
