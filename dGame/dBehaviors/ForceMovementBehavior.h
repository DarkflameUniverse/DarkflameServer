#pragma once
#include "Behavior.h"

class ForceMovementBehavior final : public Behavior
{
public:
	Behavior* m_hitAction;

	Behavior* m_hitEnemyAction;

	Behavior* m_hitFactionAction;

	float_t  m_Duration;
	float_t m_Forward;
	float_t m_Left;
	float_t m_Yaw;

	/*
	 * Inherited
	 */

	explicit ForceMovementBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void SyncCalculation(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;

};
