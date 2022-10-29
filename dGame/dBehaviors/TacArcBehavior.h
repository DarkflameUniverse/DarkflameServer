#pragma once
#include "Behavior.h"
#include "dCommonVars.h"
#include "NiPoint3.h"

class TacArcBehavior final : public Behavior
{
public:
	bool m_usePickedTarget;

	Behavior* m_action;

	bool m_checkEnv;

	Behavior* m_missAction;

	Behavior* m_blockedAction;

	float m_minDistance;

	float m_maxDistance;

	uint32_t m_maxTargets;

	bool m_targetEnemy;

	bool m_targetFriend;

	bool m_targetTeam;

	float m_angle;

	float m_upperBound;

	float m_lowerBound;

	float m_farHeight;

	float m_farWidth;

	uint32_t m_method;

	NiPoint3 m_offset;

	/*
	 * Inherited
	 */

	explicit TacArcBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
