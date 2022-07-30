#pragma once
#include "Behavior.h"

class DamageReductionBehavior final : public Behavior
{
public:
	uint32_t m_ReductionAmount;

	/*
	 * Inherited
	 */

	explicit DamageReductionBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;

	void Load() override;
};
