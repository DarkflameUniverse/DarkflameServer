#ifndef CHOICEBUILDCOMPONENT_H
#define CHOICEBUILDCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Choice build, as the client's LWOChoiceBuildComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class ChoiceBuildComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::CHOICE_BUILD;

	ChoiceBuildComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // CHOICEBUILDCOMPONENT_H
