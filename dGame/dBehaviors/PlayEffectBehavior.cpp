#include "PlayEffectBehavior.h"

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"

#include "Recorder.h"

void PlayEffectBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	const auto& target = branch.target == LWOOBJID_EMPTY ? context->originator : branch.target;

	Cinema::Recording::Recorder::RegisterEffectForActor(target, this->m_effectId);
	
	// On managed behaviors this is handled by the client
	if (!context->unmanaged)
		return;

	PlayFx(u"", target);
}

void PlayEffectBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	const auto& target = branch.target == LWOOBJID_EMPTY ? context->originator : branch.target;

	//PlayFx(u"", target);
}

void PlayEffectBehavior::Load() {
}
