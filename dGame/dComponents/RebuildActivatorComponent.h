#ifndef REBUILDACTIVATORCOMPONENT_H
#define REBUILDACTIVATORCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Rebuild activator, as the client's LWORebuildActivatorComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class RebuildActivatorComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::REBUILD_ACTIVATOR;

	RebuildActivatorComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // REBUILDACTIVATORCOMPONENT_H
