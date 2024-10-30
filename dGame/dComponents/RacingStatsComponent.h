#ifndef RACINGSTATSCOMPONENT_H
#define RACINGSTATSCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

class RacingStatsComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::RACING_STATS;

	RacingStatsComponent(Entity* parent) : Component(parent) {}
};

#endif  //!RACINGSTATSCOMPONENT_H
