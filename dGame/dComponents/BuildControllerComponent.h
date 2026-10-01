#ifndef BUILDCONTROLLERCOMPONENT_H
#define BUILDCONTROLLERCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Build controller, as the client's LWOBuildControllerComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class BuildControllerComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::REBUILD_START;

	BuildControllerComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // BUILDCONTROLLERCOMPONENT_H
