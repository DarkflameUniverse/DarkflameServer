#pragma once
#include "Behavior.h"

class TargetCasterBehavior final : public Behavior
{
public:
	Behavior* m_action;

	/*
	 * Inherited
	 */

	explicit TargetCasterBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void UnCast(BehaviorContext* context, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Load() override;
};
