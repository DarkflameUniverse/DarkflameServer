#pragma once

#include "Behavior.h"

#include <vector>

class ChangeOrientationBehavior final : public Behavior
{
public:
	bool m_OrientCaster;
	bool m_ToTarget;

	/*
	 * Inherited
	 */

	explicit ChangeOrientationBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
