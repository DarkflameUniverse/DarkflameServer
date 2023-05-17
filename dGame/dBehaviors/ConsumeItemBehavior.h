#pragma once
#include "Behavior.h"

class ConsumeItemBehavior final : public Behavior
{
public:
	explicit ConsumeItemBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {}
	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
	void Load() override;

private:
	LOT m_ConsumeLOT;
	uint32_t m_NumToConsume;
	Behavior* m_ActionNotConsumed;
	Behavior* m_ActionConsumed;
};
