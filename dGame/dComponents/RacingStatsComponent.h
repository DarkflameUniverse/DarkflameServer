#ifndef __RACINGSTATSCOMPONENT__H__
#define __RACINGSTATSCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class RacingStatsComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::RACING_STATS;

	RacingStatsComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif  //!__RACINGSTATSCOMPONENT__H__
