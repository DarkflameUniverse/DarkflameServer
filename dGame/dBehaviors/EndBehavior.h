#pragma once
#include "Behavior.h"

class EndBehavior final : public Behavior
{
public:
	uint32_t m_startBehavior;

	/*
	 * Inherited
	 */

	explicit EndBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
