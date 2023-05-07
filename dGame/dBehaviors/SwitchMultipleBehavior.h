#pragma once
#include "Behavior.h"

#include <vector>

class SwitchMultipleBehavior final : public Behavior
{
public:
	std::vector<std::pair<float, Behavior*>> m_behaviors;

	/*
	 * Inherited
	 */

	explicit SwitchMultipleBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Load() override;
};
