#pragma once
#include "Behavior.h"

class VerifyBehavior final : public Behavior
{
public:
	bool m_rangeCheck;

	bool m_blockCheck;

	float m_range;

	Behavior* m_action;

	/*
	 * Inherited
	 */

	explicit VerifyBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
