#pragma once
#include "Behavior.h"

class PropertyTeleportBehavior final : public Behavior
{
public:
	/*
	 * Inherited
	 */

	explicit PropertyTeleportBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override;

	void Load() override;

private:
	LWOMAPID m_MapId;
	bool m_CancelIfInteracting;
};
