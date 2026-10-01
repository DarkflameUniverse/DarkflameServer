#ifndef SPRINGPADCOMPONENT_H
#define SPRINGPADCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Spring pad, as the client's LWOSpringpadComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class SpringpadComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SPRINGPAD;

	SpringpadComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // SPRINGPADCOMPONENT_H
