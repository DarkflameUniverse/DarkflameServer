#pragma once

#include "Behavior.h"

#include <vector>

class CarBoostBehavior final : public Behavior
{
public:
	Behavior* m_Action;
	float m_Time;

	/*
	 * Inherited
	 */

	explicit CarBoostBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
