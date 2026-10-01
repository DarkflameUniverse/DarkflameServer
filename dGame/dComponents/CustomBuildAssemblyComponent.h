#ifndef CUSTOMBUILDASSEMBLYCOMPONENT_H
#define CUSTOMBUILDASSEMBLYCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Custom build assembly, as the client's LWOCustomBuildAssemblyComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class CustomBuildAssemblyComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::CUSTOM_BUILD_ASSEMBLY;

	CustomBuildAssemblyComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // CUSTOMBUILDASSEMBLYCOMPONENT_H
