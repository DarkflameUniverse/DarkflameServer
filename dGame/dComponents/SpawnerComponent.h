#ifndef SPAWNERCOMPONENT_H
#define SPAWNERCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * The spawner component (10) of a spawner object (LOT 176), made from the registry as the client makes it.
 */
class SpawnerComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SPAWNER;

	SpawnerComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // SPAWNERCOMPONENT_H
