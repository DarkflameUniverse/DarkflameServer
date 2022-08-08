#include "PlayEffectBehavior.h"

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"

void PlayEffectBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	// On managed behaviors this is handled by the client
	if (!context->unmanaged)
		return;

	const auto& target = branch.target == LWOOBJID_EMPTY ? context->originator : branch.target;

	PlayFx(u"", target);
}

void PlayEffectBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	const auto& target = branch.target == LWOOBJID_EMPTY ? context->originator : branch.target;

	//PlayFx(u"", target);
}

void PlayEffectBehavior::Load() {
}
