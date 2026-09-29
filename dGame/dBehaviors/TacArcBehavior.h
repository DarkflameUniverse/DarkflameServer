#pragma once
#include "Behavior.h"
#include "dCommonVars.h"
#include "NiPoint3.h"
#include <forward_list>
#include <set>
#include <vector>

class TacArcBehavior final : public Behavior {
public:
	explicit TacArcBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {}
	void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;
	void Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;
	void Load() override;

	// Reads the target count and ids the way the client writes them: at most maxTargets, returned ascending and
	// without empty ids. False when the data is cut short or lists too many targets.
	static bool ReadTargets(RakNet::BitStream& bitStream, uint32_t maxTargets, std::set<LWOOBJID>& targets);

	// Writes the closest maxTargets of closestFirst as the client does and returns them in the order their action
	// data follows (ascending id)
	static std::set<LWOOBJID> WriteTargets(RakNet::BitStream& bitStream, const std::vector<LWOOBJID>& closestFirst, uint32_t maxTargets);
private:
	float m_maxRange;
	float m_height;
	float m_distanceWeight;
	float m_angleWeight;
	float m_angle;
	float m_minRange;
	NiPoint3 m_offset;
	uint32_t m_method;
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
