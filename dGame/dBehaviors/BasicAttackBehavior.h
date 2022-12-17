#pragma once
#include "Behavior.h"

class BasicAttackBehavior final : public Behavior
{
public:
	explicit BasicAttackBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
private:
	uint32_t m_MinDamage;

	uint32_t m_MaxDamage;

	Behavior* m_OnSuccess;

	Behavior* m_OnFailArmor;
};
