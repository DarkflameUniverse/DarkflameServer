#pragma once
#include "Behavior.h"

/**
 * Behavior that casts explicit events on its target
 */
class SkillEventBehavior final : public Behavior {
public:
	explicit SkillEventBehavior(const uint32_t behaviorID) : Behavior(behaviorID) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
};

