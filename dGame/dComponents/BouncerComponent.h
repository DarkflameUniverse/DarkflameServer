#ifndef BOUNCERCOMPONENT_H
#define BOUNCERCOMPONENT_H

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "Entity.h"
#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Attached to bouncer entities, allowing other entities to bounce off of it
 */
class BouncerComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::BOUNCER;

	BouncerComponent(Entity* parentEntity);

	void Startup() override;
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Sets whether or not this bouncer needs to be activated by a pet
	 * @param value whether or not this bouncer needs to be activated by a pet
	 */
	void SetBounceOnCollision(bool value);

	/**
	 * Sets whether or not this bouncer is currently being activated by a pet, allowing entities to bounce off of it,
	 * also displays FX accordingly.
	 * @param value whether or not this bouncer is activated by a pet
	 */
	void SetBouncerEnabled(bool value);

	/**
	 * Finds the switch used to activate this bouncer if its pet-enabled and stores this components' state there
	 */
	void LookupPetSwitch();

private:
	/**
	 * Whether this bouncer is currently being activated by a pet
	 */
	bool m_BounceOnCollision;

	bool m_DirtyBounceInfo;
};

#endif // BOUNCERCOMPONENT_H
