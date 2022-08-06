#pragma once
#include "Behavior.h"

class SkillCastFailedBehavior final : public Behavior
{
public:

	/*
	 * Inherited
	 */
	explicit SkillCastFailedBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
