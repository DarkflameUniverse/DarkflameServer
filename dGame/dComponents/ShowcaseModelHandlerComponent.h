#ifndef SHOWCASEMODELHANDLERCOMPONENT_H
#define SHOWCASEMODELHANDLERCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Showcase model handler, as the client's LWOShowcaseModelHandlerComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class ShowcaseModelHandlerComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SHOWCASE_MODEL_HANDLER;

	ShowcaseModelHandlerComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // SHOWCASEMODELHANDLERCOMPONENT_H
