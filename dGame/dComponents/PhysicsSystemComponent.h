#ifndef PHYSICSSYSTEMCOMPONENT_H
#define PHYSICSSYSTEMCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Physics system, as the client's physics system component makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class PhysicsSystemComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PHYSICS_SYSTEM;

	PhysicsSystemComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // PHYSICSSYSTEMCOMPONENT_H
