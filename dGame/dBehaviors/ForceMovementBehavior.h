#pragma once
#include "Behavior.h"

class ForceMovementBehavior final : public Behavior
{
public:
	Behavior* m_hitAction;

	Behavior* m_hitEnemyAction;

	Behavior* m_hitFactionAction;
	
	/*
	 * Inherited
	 */

	explicit ForceMovementBehavior(const uint32_t behaviorId) : Behavior(behaviorId)
	{
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
	
	void Load() override;
	
};
