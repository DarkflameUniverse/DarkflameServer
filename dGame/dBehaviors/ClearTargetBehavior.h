#pragma once
#include "Behavior.h"

class ClearTargetBehavior final : public Behavior
{
public:
	Behavior* m_action;

	bool m_clearIfCaster;

	/*
	 * Inherited
	 */
	explicit ClearTargetBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
