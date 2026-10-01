#ifndef ROCKETLANDINGCOMPONENT_H
#define ROCKETLANDINGCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Rocket landing, as the client's LWORocketAnimationControlComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class RocketLandingComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ROCKET_LANDING;

	RocketLandingComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // ROCKETLANDINGCOMPONENT_H
