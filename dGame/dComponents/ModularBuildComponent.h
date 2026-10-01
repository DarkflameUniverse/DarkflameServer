#ifndef MODULARBUILDCOMPONENT_H
#define MODULARBUILDCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Modular build (rocket and car building), as the client's LWOModularBuildComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class ModularBuildComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::REBUILD;

	ModularBuildComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // MODULARBUILDCOMPONENT_H
