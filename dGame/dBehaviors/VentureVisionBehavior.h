#pragma once

#ifndef __VENTUREVISIONBEHAVIOR__H__
#define __VENTUREVISIONBEHAVIOR__H__

#include "Behavior.h"

class VentureVisionBehavior final : public Behavior
{
public:
	bool m_show_pet_digs;

	bool m_show_minibosses;

	bool m_show_collectibles;

	const std::string m_ShowCollectibles = "bShowCollectibles";

	const std::string m_ShowMiniBosses = "bShowMiniBosses";

	const std::string m_ShowPetDigs = "bShowPetDigs";


	/*
	 * Inherited
	 */

	explicit VentureVisionBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {
	}

	void Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) override;

	void UnCast(BehaviorContext* context, BehaviorBranchContext branch) override;

	void Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) override;

	void Load() override;
};

#endif  //!__VENTUREVISIONBEHAVIOR__H__
