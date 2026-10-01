#ifndef VEHICLEPHYSICSCOMPONENT_H
#define VEHICLEPHYSICSCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Vehicle physics (not the havok vehicle physics, 30), as LWOVehiclePhysicsComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class VehiclePhysicsComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::VEHICLE_PHYSICS;

	VehiclePhysicsComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // VEHICLEPHYSICSCOMPONENT_H
