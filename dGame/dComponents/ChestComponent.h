#ifndef CHESTCOMPONENT_H
#define CHESTCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Chest, as the client's LWOChestComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class ChestComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::CHEST;

	ChestComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // CHESTCOMPONENT_H
