#pragma once

#include "Behavior.h"
#include <vector>

class ChangeOrientationBehavior final : public Behavior {
public:
	explicit ChangeOrientationBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {}
	void Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;
	void Load() override;
private:
	bool m_orientCaster;
	bool m_toTarget;
	bool m_toAngle;
	float m_angle;
	bool m_relative;
};
