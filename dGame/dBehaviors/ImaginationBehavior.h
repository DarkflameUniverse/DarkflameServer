#pragma once
#include "Behavior.h"

class ImaginationBehavior final : public Behavior
{
public:
	int32_t m_imagination;

	/*
	 * Inherited
	 */

	explicit ImaginationBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Load() override;
};
