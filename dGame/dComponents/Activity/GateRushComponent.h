#ifndef GATERUSHCOMPONENT_H
#define GATERUSHCOMPONENT_H

#include "BaseRacingControlComponent.h"
#include "eReplicaComponentType.h"

/**
 * Gate rush control, as the client's LWOGateRushControlComponent makes it from the registry: a racing control
 * (BaseRacingControlComponent). Nothing of it is sent or done on the server yet.
 */
class GateRushComponent final : public BaseRacingControlComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::GATE_RUSH_CONTROL;

	GateRushComponent(Entity* parent, const int32_t componentID) : BaseRacingControlComponent(parent, componentID) {}
};

#endif // GATERUSHCOMPONENT_H
