#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"
#include "eReplicaComponentType.h"

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
class PossessionComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::POSSESSION;

	PossessionComponent(Entity* parent);
	~PossessionComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * @brief Mounts the entity
	 *
	 * @param mount Entity to be mounted
	 */
	void Mount(Entity* mount);

	/**
	 * @brief Dismounts the entity
	 *
	 * @param mount Entity to be dismounted
	 * @param forceDismount Should we forcibly dismount the entity
	 */
	void Dismount(Entity* mount, bool forceDismount = false);

	/**
	 * Sets the ID that this entity is possessing
	 * @param value The ID that this entity is possessing
	 */
	void SetPossessable(const LWOOBJID& value) {
		if (m_Possessable == value) return;
		m_Possessable = value;
		m_DirtyPossesor = true;
	}

	/**
	 * Returns the entity that this entity is currently posessing
	 * @return The entity that this entity is currently posessing
	 */
	LWOOBJID GetPossessable() const { return m_Possessable; }

	/**
	 * Sets if we are busy dismounting
	 * @param value If we are busy dismounting
	 */
	void SetIsDismounting(bool value) { m_IsDismounting = value; }

	/**
	 * Returns if we are busy dismounting
	 * @return If we are busy dismounting
	 */
	bool GetIsDismounting() const { return m_IsDismounting; }

	/**
	 * Sets the possesible type that's currently used, merely used by the shooting gallery if it's 0
	 * @param value The possesible type to set
	 */
	void SetPossessableType(ePossessionType value) {
		if (m_PossessableType == value) return;
		m_PossessableType = value;
		m_DirtyPossesor = true;
	}


	/**
	 * Gets the object ID of the mount item that is being used
	 * @return The object ID of the mount item that is being used
	 */
	LWOOBJID GetMountItemID() const { return m_MountItemID; }

	/**
	 * Sets the object ID of the mount item that is being used
	 * @param m_MountItemID The object ID of the mount item that is being used
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
	 * @brief If the possession is dirty
	 *
	 */
	bool m_DirtyPossesor = false;

	/**
	 * @brief If the possession is busy dismounting
	 *
	 */
	bool m_IsDismounting = false;

	/**
	 * Mount Item ID
	 */
	LWOOBJID m_MountItemID = LWOOBJID_EMPTY;
};
