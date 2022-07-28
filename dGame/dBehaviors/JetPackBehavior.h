#pragma once
#include "Behavior.h"

class JetPackBehavior final : public Behavior
{
public:
	int32_t m_WarningEffectID;
	float m_Airspeed;
	float m_MaxAirspeed;
	float m_VerticalVelocity;
	bool m_EnableHover;
	bool m_BypassChecks = true; // from what I can tell this defaulted true in live

	/*
	 * Inherited
	 */

	explicit JetPackBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void UnCast(BehaviorContext* context, BehaviorBranchContext branch) override;

	void Calculate(BehaviorContext* context, RakNet::BitStream* bit_stream, BehaviorBranchContext branch) override;

	void Load() override;
};
