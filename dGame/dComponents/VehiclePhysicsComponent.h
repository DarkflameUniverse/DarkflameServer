#ifndef VEHICLEPHYSICSCOMPONENT_H
#define VEHICLEPHYSICSCOMPONENT_H

#include "Entity.h"
#include "PhysicsComponent.h"
#include "eReplicaComponentType.h"

/**
 * Vehicle physics (not the havok vehicle physics, 30), as LWOVehiclePhysicsComponent makes it from the registry; like the client's, a physics component (LWOBasePhysComponent). Nothing of it is sent or done on the server yet.
 */
class VehiclePhysicsComponent final : public PhysicsComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::VEHICLE_PHYSICS;

	VehiclePhysicsComponent(Entity* parent, const int32_t componentID) : PhysicsComponent(parent, componentID) {
		m_Position = m_Parent->GetDefaultPosition();
		m_Rotation = m_Parent->GetDefaultRotation();
	}
};

#endif // VEHICLEPHYSICSCOMPONENT_H
