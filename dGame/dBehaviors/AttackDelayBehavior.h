#pragma once
#include "Behavior.h"

class AttackDelayBehavior final : public Behavior
{
public:
	Behavior* m_action;

	float m_delay;

	uint32_t m_numIntervals;

	bool m_ignoreInterrupts;

	/*
	 * Inherited
	 */

	explicit AttackDelayBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void SyncCalculation(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
