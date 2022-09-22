#pragma once
#include "Behavior.h"

class SpeedBehavior final : public Behavior
{
public:

	/*
	 * Inherited
	 */
	explicit SpeedBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;

	void End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;

	void Load() override;

private:
	float m_RunSpeed;

	bool m_AffectsCaster;
};
