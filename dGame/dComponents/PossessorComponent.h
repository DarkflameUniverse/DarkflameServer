#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"

// possession types
enum class ePossessionType : uint8_t {
	NO_POSSESSION = 0,
	ATTACHED_VISIBLE,
	NOT_ATTACHED_VISIBLE,
	NOT_ATTACHED_NOT_VISIBLE,
};

/**
 * Represents an entity that can posess other entities. Generally used by players to drive a car.
 */
class PossessorComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_POSSESSOR;

	PossessorComponent(Entity* parent);
	~PossessorComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * @brief mounts the entity
	 *
	 * @param mount entity to be mounted
	 */
	void Mount(Entity* mount);

	/**
	 * @brief dismounts the entity
	 *
	 * @param mount entity to be dismounted
	 * @param forceDismount Should we forcibly dismount the entity
	 */
	void Dismount(Entity* mount, bool forceDismount = false);

	/**
	 * Sets the entity that this entity is possessing
	 * @param value the ID of the entity this ID should posess
	 */
	void SetPossessable(LWOOBJID value) { m_Possessable = value; m_DirtyPossesor = true; }

	/**
	 * Returns the entity that this entity is currently posessing
	 * @return the entity that this entity is currently posessing
	 */
	LWOOBJID GetPossessable() const { return m_Possessable; }

	/**
	 * Sets if we are busy dismounting
	 * @param value if we are busy dismounting
	 */
	void SetIsDismounting(bool value) { m_IsDismounting = value; }

	/**
	 * Returns if we are busy dismounting
	 * @return if we are busy dismounting
	 */
	bool GetIsDismounting() const { return m_IsDismounting; }

	/**
	 * Sets the possesible type that's currently used, merely used by the shooting gallery if it's 0
	 * @param value the possesible type to set
	 */
	void SetPossessableType(ePossessionType value) { m_PossessableType = value; m_DirtyPossesor = true; }


	/**
	 * Gets the object ID of the mount item that is being used
	 * @return the object ID of the mount item that is being used
	 */
	LWOOBJID GetMountItemID() const { return m_MountItemID; }

	/**
	 * Sets the object ID of the mount item that is being used
	 * @param m_MountItemID the object ID of the mount item that is being used
	 */
	void SetMountItemID(LWOOBJID mountItemID) { m_MountItemID = mountItemID; }

private:

	/**
	 * The ID of the entity this entity is possessing (e.g. the ID of a car)
	 */
	LWOOBJID m_Possessable = LWOOBJID_EMPTY;

	/**
	 * @brief possessable type
	 *
	 */
	ePossessionType m_PossessableType = ePossessionType::NO_POSSESSION;

	/**
	 * @brief if the possessor is dirty
	 *
	 */
	bool m_DirtyPossesor = false;

	/**
	 * @brief if the possessor is busy dismounting
	 *
	 */
	bool m_IsDismounting = false;

	/**
	 * Mount Item ID
	 */
	LWOOBJID m_MountItemID = LWOOBJID_EMPTY;
};
