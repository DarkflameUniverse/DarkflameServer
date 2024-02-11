#ifndef __RACINGSTATSCOMPONENT__H__
#define __RACINGSTATSCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class RacingStatsComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::RACING_STATS;

	RacingStatsComponent(Entity* parent) : Component(parent) {}
};

#endif  //!__RACINGSTATSCOMPONENT__H__
