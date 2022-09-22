#pragma once
#include "Behavior.h"

class PlayEffectBehavior final : public Behavior
{
public:
	/*
	 * Inherited
	 */
	explicit PlayEffectBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
