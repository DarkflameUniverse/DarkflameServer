#ifndef PHYSICSSYSTEMCOMPONENT_H
#define PHYSICSSYSTEMCOMPONENT_H

#include "Entity.h"
#include "PhysicsComponent.h"
#include "eReplicaComponentType.h"

/**
 * Physics system, as the client's physics system component makes it from the registry; like the client's, a physics component (LWOBasePhysComponent). Nothing of it is sent or done on the server yet.
 */
class PhysicsSystemComponent final : public PhysicsComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PHYSICS_SYSTEM;

	PhysicsSystemComponent(Entity* parent, const int32_t componentID) : PhysicsComponent(parent, componentID) {
		m_Position = m_Parent->GetDefaultPosition();
		m_Rotation = m_Parent->GetDefaultRotation();
	}
};

#endif // PHYSICSSYSTEMCOMPONENT_H
