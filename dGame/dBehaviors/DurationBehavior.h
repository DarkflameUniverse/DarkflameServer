#pragma once
#include "Behavior.h"

class DurationBehavior final : public Behavior
{
public:
	float m_duration;

	Behavior* m_action;

	explicit DurationBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
private:
	float m_Timer = 0.0f;
};
