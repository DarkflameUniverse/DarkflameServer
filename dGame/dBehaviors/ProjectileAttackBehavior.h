#pragma once
#include "Behavior.h"
#include "NiPoint3.h"

class ProjectileAttackBehavior final : public Behavior
{
public:
	LOT m_lot;

	uint32_t m_projectileCount;

	float m_projectileSpeed;

	float m_maxDistance;

	float m_spreadAngle;

	NiPoint3 m_offset;

	bool m_trackTarget;

	float m_trackRadius;

	bool m_useMouseposit;

	/*
	 * Inherited
	 */

	explicit ProjectileAttackBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
