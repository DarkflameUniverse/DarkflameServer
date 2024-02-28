#pragma once
#include "Behavior.h"

class ImmunityBehavior final : public Behavior
{
public:
	/*
	 * Inherited
	 */

	explicit ImmunityBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;

	void Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;

	void Load() override;

private:
	// stuns
	bool m_ImmuneToStunAttack = false;
	bool m_ImmuneToStunEquip = false;
	bool m_ImmuneToStunInteract = false;
	bool m_ImmuneToStunJump = false; // Unused
	bool m_ImmuneToStunMove = false;
	bool m_ImmuneToStunTurn = false;
	bool m_ImmuneToStunUseItem = false; // Unused

	//status
	bool m_ImmuneToBasicAttack = false;
	bool m_ImmuneToDamageOverTime = false;
	bool m_ImmuneToKnockback = false;
	bool m_ImmuneToInterrupt = false;
	bool m_ImmuneToSpeed = false;
	bool m_ImmuneToImaginationGain = false;
	bool m_ImmuneToImaginationLoss = false;
	bool m_ImmuneToQuickbuildInterrupt = false;
	bool m_ImmuneToPullToPoint = false; // Unused in cdclient, but used in client
};
