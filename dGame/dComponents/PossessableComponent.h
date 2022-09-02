#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"
#include "Item.h"
#include "PossessorComponent.h"
#include "eAninmationFlags.h"

/**
 * Represents an entity that can be controlled by some other entity, generally used by cars to indicate that some
 * player is controlling it.
 */
class PossessableComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_POSSESSABLE;

	PossessableComponent(Entity* parentEntity, uint32_t componentId);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * @brief mounts the Entity
	 */
	void Mount();

	/**
	 * @brief dismounts the Entity
	 */
	void Dismount();

	/**
	 * Sets the possessor of this Entity
	 * @param value the ID of the possessor to set
	 */
	void SetPossessor(LWOOBJID value) { m_Possessor = value; m_DirtyPossessable = true; };

	/**
	 * Returns the possessor of this Entity
	 * @return the possessor of this Entity
	 */
	LWOOBJID GetPossessor() const { return m_Possessor; };

	/**
	 * Sets the animation Flag of the possessable
	 * @param value the animation flag to set to
	 */
	void SetAnimationFlag(eAnimationFlags value) { m_AnimationFlag = value; m_DirtyPossessable = true; };

	/**
	 * Returns the possession type of this Entity
	 * @return the possession type of this Entity
	 */
	ePossessionType GetPossessionType() const { return m_PossessionType; };

	/**
	 * Returns if the Entity should deposses on hit
	 * @return if the Entity should deposses on hit
	 */
	bool GetDepossessOnHit() const { return m_DepossessOnHit; };

	/**
	 * Forcibly depossess the Entity
	 */
	void ForceDepossess() { m_ImmediatelyDepossess = true; m_DirtyPossessable = true; };

	/**
	 * Set if the parent entity was spawned from an item
	 * @param value if the parent entity was spawned from an item
	 */
	void SetIsItemSpawned(bool value) { m_ItemSpawned = value; };

	/**
	 * Returns if the parent entity was spawned from an item
	 * @return if the parent entity was spawned from an item
	 */
	LWOOBJID GetIsItemSpawned() const { return m_ItemSpawned; };

	/**
	 * Handles an OnUsed event by some other entity, if said entity has a Possessor it becomes the possessor
	 * of this entity
	 * @param originator the entity that caused the event to trigger
	 */
	void OnUse(Entity* originator) override;

private:

	/**
	 * @brief Whether the possessor is dirty
	 */
	bool m_DirtyPossessable = true;

	/**
	 * @brief The possessor of this entity, e.g. the entity that controls this entity
	 */
	LWOOBJID m_Possessor = LWOOBJID_EMPTY;

	/**
	 * @brief The type of possesstion to use on this entity
	 */
	ePossessionType m_PossessionType = ePossessionType::NO_POSSESSION;

	/**
	 * @brief Should the possessable be dismount on hit
	 */
	bool m_DepossessOnHit = false;

	/**
	 * @brief What animaiton flag to use
	 *
	 */
	eAnimationFlags m_AnimationFlag = eAnimationFlags::IDLE_INVALID;

	/**
	 * @brief Should this be immediately depossessed
	 *
	 */
	bool m_ImmediatelyDepossess = false;

	/**
	 * @brief Whether the parent entity was spawned from an item
	 *
	 */
	bool m_ItemSpawned = false;
};
