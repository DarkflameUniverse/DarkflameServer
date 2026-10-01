#ifndef MODELBUILDERCOMPONENT_H
#define MODELBUILDERCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Model builder, as the client's LWOModelBuilderComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class ModelBuilderComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MODEL_BUILDER;

	ModelBuilderComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // MODELBUILDERCOMPONENT_H
