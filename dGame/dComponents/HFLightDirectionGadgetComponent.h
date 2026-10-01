#ifndef HFLIGHTDIRECTIONGADGETCOMPONENT_H
#define HFLIGHTDIRECTIONGADGETCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * The Happy Flower editor's light direction gadget (66), as the client makes it from the registry (LOT 6968). An
 * editor-only object: nothing of it is sent or done on the server.
 */
class HFLightDirectionGadgetComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::HF_LIGHT_DIRECTION_GADGET;

	HFLightDirectionGadgetComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // HFLIGHTDIRECTIONGADGETCOMPONENT_H
