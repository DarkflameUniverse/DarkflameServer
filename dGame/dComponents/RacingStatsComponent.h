#ifndef __RACINGSTATSCOMPONENT__H__
#define __RACINGSTATSCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class RacingStatsComponent final : public Component {
public:
	constexpr static const eReplicaComponentType ComponentType = eReplicaComponentType::RACING_STATS;

	RacingStatsComponent(const LWOOBJID& parentEntityId) : Component{ parentEntityId } {}
};

#endif  //!__RACINGSTATSCOMPONENT__H__
