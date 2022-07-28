#ifndef BOUNCERCOMPONENT_H
#define BOUNCERCOMPONENT_H

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "Entity.h"
#include "Component.h"

/**
 * Attached to bouncer entities, allowing other entities to bounce off of it
 */
class BouncerComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_BOUNCER;

	BouncerComponent(Entity* parentEntity);
	~BouncerComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	Entity* GetParentEntity() const;

	/**
	 * Sets whether or not this bouncer needs to be activated by a pet
	 * @param value whether or not this bouncer needs to be activated by a pet
	 */
	void SetPetEnabled(bool value);

	/**
	 * Sets whether or not this bouncer is currently being activated by a pet, allowing entities to bounce off of it,
	 * also displays FX accordingly.
	 * @param value whether or not this bouncer is activated by a pet
	 */
	void SetPetBouncerEnabled(bool value);

	/**
	 * Gets whether this bouncer should be enabled using pets
	 * @return whether this bouncer should be enabled using pets
	 */
	bool GetPetEnabled() const;

	/**
	 * Gets whether this bouncer is currently activated by a pet
	 * @return whether this bouncer is currently activated by a pet
	 */
	bool GetPetBouncerEnabled() const;

	/**
	 * Finds the switch used to activate this bouncer if its pet-enabled and stores this components' state there
	 */
	void LookupPetSwitch();

private:
	/**
	 * Whether this bouncer needs to be activated by a pet
	 */
	bool m_PetEnabled;

	/**
	 * Whether this bouncer is currently being activated by a pet
	 */
	bool m_PetBouncerEnabled;

	/**
	 * Whether the pet switch for this bouncer has been located
	 */
	bool m_PetSwitchLoaded;
};

#endif // BOUNCERCOMPONENT_H
