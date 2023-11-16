#pragma once
#include "Behavior.h"

class AirMovementBehavior final : public Behavior
{
public:
	explicit AirMovementBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
private:
	float m_Timeout;
};
