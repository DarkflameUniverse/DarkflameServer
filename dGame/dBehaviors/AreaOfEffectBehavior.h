#pragma once
#include "Behavior.h"

class AreaOfEffectBehavior final : public Behavior
{
public:
	Behavior* m_action;

	uint32_t m_maxTargets;

	float m_radius;

	int32_t m_ignoreFaction;

	int32_t m_includeFaction;

	int32_t m_TargetSelf;

	int32_t m_targetEnemy;

	int32_t m_targetFriend;

	/*
	 * Inherited
	 */
	explicit AreaOfEffectBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
