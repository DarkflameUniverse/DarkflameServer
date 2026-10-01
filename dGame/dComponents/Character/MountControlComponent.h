#ifndef MOUNTCONTROLCOMPONENT_H
#define MOUNTCONTROLCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Mount control (111): the character's riding of mounts, as the client's LWOMountControlComponent: the client makes it (componentID -1) with every character
 * (LWOCharacterComponent::Factory). Nothing of it is sent or done on the server yet.
 */
class MountControlComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MOUNT_CONTROL;

	MountControlComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // MOUNTCONTROLCOMPONENT_H
