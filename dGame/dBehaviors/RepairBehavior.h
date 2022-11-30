#pragma once
#include "Behavior.h"

class RepairBehavior final : public Behavior
{
public:
	uint32_t m_armor;

	/*
	 * Inherited
	 */

	explicit RepairBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Load() override;
};
