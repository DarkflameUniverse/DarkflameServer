#ifndef PETCREATORCOMPONENT_H
#define PETCREATORCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Pet creator (37): the character's taming of pets, as the client's LWOPetCreatorComponent: the client makes it (componentID -1) with every character
 * (LWOCharacterComponent::Factory). Nothing of it is sent or done on the server yet.
 */
class PetCreatorComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PET_CREATOR;

	PetCreatorComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // PETCREATORCOMPONENT_H
