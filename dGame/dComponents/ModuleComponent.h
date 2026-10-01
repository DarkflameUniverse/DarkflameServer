#ifndef MODULECOMPONENT_H
#define MODULECOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Module (a car or rocket part), as the client's LWOModuleComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class ModuleComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MODULE;

	ModuleComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // MODULECOMPONENT_H
