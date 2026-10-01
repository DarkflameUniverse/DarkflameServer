#ifndef GATERUSHCOMPONENT_H
#define GATERUSHCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Gate rush, as the client's LWOGateRushComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class GateRushComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::GATE_RUSH_CONTROL;

	GateRushComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // GATERUSHCOMPONENT_H
