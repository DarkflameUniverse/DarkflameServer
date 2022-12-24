#pragma once

#include <vector>

#include "Behavior.h"

class ApplyBuffBehavior final : public Behavior
{
public:
		/*
	 * Inherited
	 */
	explicit ApplyBuffBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void UnCast(BehaviorContext* context, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;

private:
	int32_t m_BuffId;
	float m_Duration;
	bool m_IgnoreUncast;
	bool m_TargetCaster;
	bool m_AddImmunity;
	bool m_ApplyOnTeammates;
	bool m_CancelOnDamaged;
	bool m_CancelOnDeath;
	bool m_CancelOnLogout;
	bool m_CancelOnRemoveBuff;
	bool m_CancelOnUi;
	bool m_CancelOnUnequip;
	bool m_CancelOnZone;
	bool m_CancelOnDamageAbsDone;
	bool m_UseRefCount;
};
