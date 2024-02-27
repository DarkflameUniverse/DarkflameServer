#pragma once
#include "Behavior.h"
#include <forward_list>

class AreaOfEffectBehavior final : public Behavior
{
public:
	explicit AreaOfEffectBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {}
	void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;
	void Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;
	void Load() override;
private:
	Behavior* m_action;
	uint32_t m_maxTargets;
	float m_radius;
	bool m_useTargetPosition;
	bool m_useTargetAsCaster;
	NiPoint3 m_offset;

	std::forward_list<int32_t> m_ignoreFactionList {};
	std::forward_list<int32_t> m_includeFactionList {};
	bool m_targetSelf;
	bool m_targetEnemy;
	bool m_targetFriend;
	bool m_targetTeam;
};
