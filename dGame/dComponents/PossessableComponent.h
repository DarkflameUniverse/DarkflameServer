#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"

/**
 * Represents an entity that can be controlled by some other entity, generally used by cars to indicate that some
 * player is controlling it.
 */
class PossessableComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_POSSESSABLE;
	
	PossessableComponent(Entity* parentEntity);
	~PossessableComponent() override;

    void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
    void Update(float deltaTime) override;

    /**
     * Sets the possessor of this entity
     * @param value the ID of the possessor to set
     */
    void SetPossessor(LWOOBJID value);

    /**
     * Returns the possessor of this entity
     * @return the possessor of this entitythe
     */
    LWOOBJID GetPossessor() const;

    /**
     * Handles an OnUsed event by some other entity, if said entity has a PossessorComponent it becomes the possessor
     * of this entity
     * @param originator the entity that caused the event to trigger
     */
	void OnUse(Entity* originator) override;

private:

    /**
     * The possessor of this entity, e.g. the entity that controls this entity
     */
    LWOOBJID m_Possessor;
};
