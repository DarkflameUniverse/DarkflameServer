#pragma once
#include "Behavior.h"
#include "dCommonVars.h"
#include "NiPoint3.h"
#include <forward_list>

enum class eTacArcMethod : uint32_t;

class TacArcBehavior final : public Behavior {
public:
	explicit TacArcBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {}
	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
	void Load() override;
private:
	float m_maxRange;
	float m_height;
	float m_distanceWeight;
	float m_angleWeight;
	float m_angle;
	float m_minRange;
	NiPoint3 m_offset;
	eTacArcMethod m_method;
	float m_upperBound;
	float m_lowerBound;
	bool m_usePickedTarget;
	bool m_useTargetPostion;
	bool m_checkEnv;
	bool m_useAttackPriority;
	Behavior* m_action;
	Behavior* m_missAction;
	Behavior* m_blockedAction;
	uint32_t m_maxTargets;
	float m_farHeight;
	float m_farWidth;
	float m_nearHeight;
	float m_nearWidth;

	std::forward_list<int32_t> m_ignoreFactionList {};
	std::forward_list<int32_t> m_includeFactionList {};
	bool m_targetSelf;
	bool m_targetEnemy;
	bool m_targetFriend;
	bool m_targetTeam;
};
