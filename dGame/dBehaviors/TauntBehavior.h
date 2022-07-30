#pragma once

#include "Behavior.h"

class TauntBehavior final : public Behavior
{
public:
	float m_threatToAdd;

	/*
	 * Inherited
	 */

	explicit TauntBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
