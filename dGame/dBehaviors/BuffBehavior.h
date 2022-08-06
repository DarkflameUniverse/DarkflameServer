#pragma once
#include "Behavior.h"

class BuffBehavior final : public Behavior
{
public:
	uint32_t m_health;

	uint32_t m_armor;

	uint32_t m_imagination;

	/*
	 * Inherited
	 */
	explicit BuffBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void UnCast(BehaviorContext* context, BehaviorBranchContext branch) override;

	void Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;

	void End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;

	void Load() override;
};
