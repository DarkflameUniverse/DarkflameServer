#pragma once
#include "Behavior.h"

class RemoveBuffBehavior final : public Behavior
{
public:

	/*
	 * Inherited
	 */

	explicit RemoveBuffBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;

	void Load() override;

private:
	bool m_RemoveImmunity;
	uint32_t m_BuffId;
};
