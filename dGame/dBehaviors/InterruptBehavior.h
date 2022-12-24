#pragma once
#include "Behavior.h"

class InterruptBehavior final: public Behavior
{
private:
	bool m_target;

	bool m_InterruptAttack;
	
	bool m_InterruptBlock;

	bool m_interruptCharge;

	/*
	 * Inherited
	 */
public:
	explicit InterruptBehavior(const uint32_t behavior_id): Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
