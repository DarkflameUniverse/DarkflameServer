#include "SkillCastFailedBehavior.h"

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"

void SkillCastFailedBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	context->failed = true;
}

void SkillCastFailedBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	context->failed = true;
}

void SkillCastFailedBehavior::Load() {
}
