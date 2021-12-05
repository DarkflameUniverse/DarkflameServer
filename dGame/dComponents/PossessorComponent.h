#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"

/**
 * Represents an entity that can posess other entities. Generally used by players to drive a car.
 */
class PossessorComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_POSSESSOR;
	
	PossessorComponent(Entity* parent);
	~PossessorComponent() override;

    void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
    void Update(float deltaTime) override;

    /**
     * Sets the entity that this entity is possessing
     * @param value the ID of the entity this ID should posess
     */
    void SetPossessable(LWOOBJID value);

    /**
     * Returns the entity that this entity is currently posessing
     * @return the entity that this entity is currently posessing
     */
    LWOOBJID GetPossessable() const;

private:

    /**
     * The ID of the entity this entity is possessing (e.g. the ID of a car)
     */
    LWOOBJID m_Possessable;
};
