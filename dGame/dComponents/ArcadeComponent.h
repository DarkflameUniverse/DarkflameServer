#ifndef ARCADECOMPONENT_H
#define ARCADECOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Arcade, as the client's LWOArcadeComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class ArcadeComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ARCADE;

	ArcadeComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // ARCADECOMPONENT_H
