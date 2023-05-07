#pragma once
#include "Behavior.h"

class SpawnQuickbuildBehavior final : public Behavior
{
public:

	/*
	 * Inherited
	 */
	explicit SpawnQuickbuildBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
