#pragma once
#include "Behavior.h"

class MovementSwitchBehavior final : public Behavior
{
private:
	/*
	 * Members
	 */
	Behavior* m_airAction;

	Behavior* m_doubleJumpAction;

	Behavior* m_fallingAction;

	Behavior* m_groundAction;

	Behavior* m_jetpackAction;

	Behavior* m_jumpAction;

	Behavior* m_movingAction;

	/**
	 * @brief Loads a movement type from the database into a behavior
	 * 
	 * @param movementType The movement type to lookup in the database
	 * @param behaviorToLoad The Behavior where the result will be stored
	 */
	Behavior* LoadMovementType(std::string movementType);

public:
	/*
	 * Inherited
	 */
	explicit MovementSwitchBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
