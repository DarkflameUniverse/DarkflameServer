
#pragma once
#include "Behavior.h"
#include "eAninmationFlags.h"

class ChangeIdleFlagsBehavior final : public Behavior {
public:

	/*
	 * Inherited
	 */
	explicit ChangeIdleFlagsBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {}

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
	void Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;
	void End(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;
	void Load() override;

private:
	eAnimationFlags m_FlagsOff;
	eAnimationFlags m_FlagsOn;
};
