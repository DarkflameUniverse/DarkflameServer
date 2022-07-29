#pragma once

#include <vector>

#include "Behavior.h"

class ApplyBuffBehavior final : public Behavior
{
public:
	int32_t m_BuffId;
	float m_Duration;
	bool addImmunity;
	bool cancelOnDamaged;
	bool cancelOnDeath;
	bool cancelOnLogout;
	bool cancelonRemoveBuff;
	bool cancelOnUi;
	bool cancelOnUnequip;
	bool cancelOnZone;

	/*
	 * Inherited
	 */
	explicit ApplyBuffBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void UnCast(BehaviorContext* context, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void Load() override;
};
