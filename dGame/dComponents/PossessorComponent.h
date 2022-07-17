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
		 * Sets if we are busy mounting or dismounting
		 * @param value if we are busy mounting or dismounting
		 */
		void SetIsBusy(bool value) { m_IsBusy = value; }

		/**
		 * Returns if we are busy mounting or dismounting
		 * @return if we are busy mounting or dismounting
		 */
		bool GetIsBusy() const { return m_IsBusy; }

		/**
		 * Sets the possesible type that's currently used, merely used by the shooting gallery if it's 0
		 * @param value the possesible type to set
		 */
		void SetPossessableType(ePossessionType value) { m_PossessableType = value; m_DirtyPossesor = true; }

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
		 * @brief if the possessor is busy mounting or dismounting
		 *
		 */
		bool m_IsBusy = false;
};
