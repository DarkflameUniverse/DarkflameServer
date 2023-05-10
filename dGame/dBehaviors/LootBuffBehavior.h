#pragma once
#include "Behavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "ControllablePhysicsComponent.h"

/**
 * @brief This is the behavior class to be used for all Loot Buff behavior nodes in the Behavior tree.
 *
 */
class LootBuffBehavior final : public Behavior
{
public:

	float m_Scale;

	/*
	 * Inherited
	 */

	explicit LootBuffBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void UnCast(BehaviorContext* context, BehaviorBranchContext branch) override;

	void Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;

	void Load() override;
};
