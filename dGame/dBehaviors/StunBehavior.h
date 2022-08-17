#pragma once
#include "Behavior.h"

class StunBehavior final : public Behavior
{
public:
	bool m_stunCaster;

	/*
	 * Inherited
	 */
	explicit StunBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Load() override;
};
