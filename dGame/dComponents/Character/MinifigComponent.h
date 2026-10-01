#ifndef MINIFIGCOMPONENT_H
#define MINIFIGCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * An NPC minifigure's look (MinifigComponent row: head, torso, legs, colors, decals), as the client's LWOMiniFigComponent
 * has it. The client reads it from the CDClient itself: nothing of it is sent.
 */
class MinifigComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MINIFIG;

	MinifigComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // MINIFIGCOMPONENT_H
