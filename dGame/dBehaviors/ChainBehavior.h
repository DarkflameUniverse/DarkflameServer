#pragma once

#include "Behavior.h"

#include <vector>

class ChainBehavior final : public Behavior
{
public:
	std::vector<Behavior*> m_behaviors;

	/*
	 * Inherited
	 */

	explicit ChainBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
