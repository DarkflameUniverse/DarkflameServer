#pragma once
#include "Behavior.h"

class Entity;

class KnockbackBehavior final : public Behavior
{
public:
	/*
	 * Inherited
	 */

	float m_strength;
	float m_angle;
	bool m_relative;
	uint32_t m_time;
	bool m_ignoreSelf;
	bool m_caster;


	explicit KnockbackBehavior(const uint32_t behaviorID) : Behavior(behaviorID) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;

	void Load() override;

private:
	/**
	 * Knocks back the target if the server moves it. Players knock themselves back on their own client when it
	 * runs the skill; enemies and NPCs are only drawn where the server puts them, so the server flies them.
	 */
	void KnockbackServerObject(BehaviorContext* context, const BehaviorBranchContext& branch) const;
};
