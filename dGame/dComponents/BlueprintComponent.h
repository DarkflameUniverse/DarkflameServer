#ifndef BLUEPRINTCOMPONENT_H
#define BLUEPRINTCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Blueprint, as the client's LWOBlueprintComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class BlueprintComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::BLUEPRINT;

	BlueprintComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // BLUEPRINTCOMPONENT_H
