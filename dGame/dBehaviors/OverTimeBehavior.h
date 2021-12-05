#pragma once
#include "Behavior.h"

class OverTimeBehavior final : public Behavior
{
public:
    Behavior* m_Action;
    float m_Delay;
    int32_t m_NumIntervals;

	/*
	 * Inherited
	 */

	explicit OverTimeBehavior(const uint32_t behaviorId) : Behavior(behaviorId)
	{
	}
	
	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
