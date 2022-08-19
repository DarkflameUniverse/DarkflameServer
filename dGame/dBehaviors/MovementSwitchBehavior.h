#pragma once
#include "Behavior.h"

class MovementSwitchBehavior final : public Behavior
{
public:
	/*
	 * Members
	 */
	Behavior* m_airAction;

	Behavior* m_doubleJumpAction;

	Behavior* m_fallingAction;

	Behavior* m_groundAction;

	Behavior* m_jetpackAction;

	Behavior* m_jumpAction;

	/*
	 * Inherited
	 */
	explicit MovementSwitchBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
