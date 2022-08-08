#pragma once
#include "Behavior.h"

class KnockbackBehavior final : public Behavior
{
public:
	/*
	 * Inherited
	 */

	uint32_t m_strength;
	uint32_t m_angle;
	bool m_relative;
	uint32_t m_time;


	explicit KnockbackBehavior(const uint32_t behaviorID) : Behavior(behaviorID) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
