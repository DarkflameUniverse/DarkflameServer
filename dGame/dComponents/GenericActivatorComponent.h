#ifndef GENERICACTIVATORCOMPONENT_H
#define GENERICACTIVATORCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Generic activator, as the client's LWOGenericActivatorComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class GenericActivatorComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::GENERIC_ACTIVATOR;

	GenericActivatorComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // GENERICACTIVATORCOMPONENT_H
