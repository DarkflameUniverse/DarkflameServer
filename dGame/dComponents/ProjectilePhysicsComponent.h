#ifndef PROJECTILEPHYSICSCOMPONENT_H
#define PROJECTILEPHYSICSCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Projectile physics, as LWOProjectilePhysicsComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class ProjectilePhysicsComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PROJECTILE_PHYSICS;

	ProjectilePhysicsComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // PROJECTILEPHYSICSCOMPONENT_H
