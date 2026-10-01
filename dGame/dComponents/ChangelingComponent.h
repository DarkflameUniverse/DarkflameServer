#ifndef CHANGELINGCOMPONENT_H
#define CHANGELINGCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Changeling, as the client's LWOChangelingComponent makes it from the registry. Nothing of it is sent or done on the server yet.
 */
class ChangelingComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::CHANGLING;

	ChangelingComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // CHANGELINGCOMPONENT_H
