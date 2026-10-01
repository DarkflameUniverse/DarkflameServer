#ifndef PLATFORMBOUNDARYCOMPONENT_H
#define PLATFORMBOUNDARYCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * A moving platform's boundary, as the client's LWOPlatformBoundaryComponent; the server sends nothing for it.
 */
class PlatformBoundaryComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PLATFORM_BOUNDARY;

	PlatformBoundaryComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // PLATFORMBOUNDARYCOMPONENT_H
