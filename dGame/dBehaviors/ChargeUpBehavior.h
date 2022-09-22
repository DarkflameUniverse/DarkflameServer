#pragma once
#include "Behavior.h"

class ChargeUpBehavior final : public Behavior
{
public:
	Behavior* m_action;

	/*
	 * Inherited
	 */

	explicit ChargeUpBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
