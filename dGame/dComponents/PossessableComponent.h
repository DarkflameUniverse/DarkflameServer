#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"
#include "Item.h"
#include "PossessorComponent.h"

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
		 * Sets the possessor of this entity
		 * @param value the ID of the possessor to set
		 */
		void SetPossessor(LWOOBJID value) { m_Possessor = value; };

		/**
		 * Returns the possessor of this entity
		 * @return the possessor of this entity
		 */
		LWOOBJID GetPossessor() const { return m_Possessor; };

		/**
		 * Sets the possessor of this entity
		 * @param value the ID of the possessor to set
		 */
		void SetAnimationFlag(eAnimationFlags value) { m_AnimationFlag = value; };


		/**
		 * Returns the possession type of this entity
		 * @return the possession type of this entity
		 */
		ePossessionType GetPossessionType() const { return m_PossessionType; };

		/**
		 * Handles an OnUsed event by some other entity, if said entity has a CharacterComponent it becomes the possessor
		 * of this entity
		 * @param originator the entity that caused the event to trigger
		 */
		void OnUse(Entity* originator) override;

	private:

		/**
		 * The possessor of this entity, e.g. the entity that controls this entity
		 */
		LWOOBJID m_Possessor;

		/**
		 * The type of possesstion to use on this entity
		 */
		ePossessionType m_PossessionType = ePossessionType::NO_POSSESSION;

		/**
		 * @brief What animaiton flag to use
		 * 
		 */
		eAnimationFlags m_AnimationFlag = eAnimationFlags::IDLE_INVALID;

};
