#ifndef PACKAGECOMPONENT_H
#define PACKAGECOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Package, as the client's LWOPackageComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class PackageComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PACKAGE;

	PackageComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // PACKAGECOMPONENT_H
