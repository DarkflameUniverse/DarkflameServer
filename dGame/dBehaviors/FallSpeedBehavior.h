#pragma once
#include "Behavior.h"

class FallSpeedBehavior final : public Behavior
{
public:
	explicit FallSpeedBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {}

	void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;
	void Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;
	void Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;
	void End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;
	void UnCast(BehaviorContext* context, BehaviorBranchContext branch) override;
	void Load() override;

private:
	float m_PercentSlowed;
};
