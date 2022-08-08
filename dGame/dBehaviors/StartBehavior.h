#pragma once
#include "Behavior.h"

class StartBehavior final : public Behavior
{
public:
	Behavior* m_action;

	/*
	 * Inherited
	 */

	explicit StartBehavior(const uint32_t behaviorID) : Behavior(behaviorID) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
