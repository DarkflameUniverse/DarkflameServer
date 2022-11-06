#pragma once
#include "Behavior.h"

class BasicAttackBehavior final : public Behavior
{
public:
	uint32_t m_minDamage;

	uint32_t m_maxDamage;

	Behavior* m_onSuccess;

	explicit BasicAttackBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
