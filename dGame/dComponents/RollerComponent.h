#ifndef ROLLERCOMPONENT_H
#define ROLLERCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Roller, as the client's LWORollerComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class RollerComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ROLLER;

	RollerComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // ROLLERCOMPONENT_H
