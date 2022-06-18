#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"
#include "Item.h"

enum class ePossessionType : uint8_t {
	NO_POSSESSION = 0,
	ATTACHED_VISIBLE,
	NOT_ATTACHED_VISIBLE,
	NOT_ATTACHED_NOT_VISIBLE,
};

/**
 * Represents an entity that can possess other entities. Generally used by players to drive a car.
 */
class PossessorComponent : public Component {
	public:
		static const uint32_t ComponentType = COMPONENT_TYPE_POSSESSOR;

		PossessorComponent(Entity* parent);

		void Mount(Item* item);
		void Dismount(Item* objectID);

		/**
		 * @brief Serialize the Component to be sent
		 * 
		 * @param outBitStream The Bitstream that is being written to
		 * @param bIsInitialUpdate Whether this is the initial update
		 * @param flags Flags that are being passed
		 */
		void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

		/**
		 * Gets whether the possessor currently dismounting
		 * @return whether the possessor currently dismounting
		 */
		const bool GetIsDismounting() const { return m_IsDismounting; }

		/**
		 * Sets the state of whether the possessor currently dismounting
		 * @param IsDismounting whether the possessor currently dismounting
		 */
		void SetIsDismounting(bool IsDismounting) { m_IsDismounting = IsDismounting; }

		/**
		 * Gets if the entity has a dirty possessor
		 * @return whether the entity has a dirty possessor
		 */
		const bool GetDirtyPossesor() const { return m_DirtyPossesor; }

		/**
		 * Sets the state of whether the character possessable has changed
		 * @param DirtyPossesor whether the character possessable has changed
		 */
		void SetDirtyPossesor(bool DirtyPossesor) { m_DirtyPossesor = DirtyPossesor; }

		/**
		 * Gets the (optional) object ID of the possessable the character is currently in
		 * @return the object ID of the vehilce the character is in
		 */
		const LWOOBJID GetPossessable() const { return m_Possessable; }

		/**
		 * Sets the item that is being possessed
		 * @param value the item that is being possessed
		 */
		void SetPossesableItem(Item* value) { m_PossesableItem = value; };

		/**
		 * Returns the item that is being possessed
		 * @return The item that is being possessed
		 */
		Item* GetPossesableItem() const { return m_PossesableItem; };

		/**
		 * Sets the (optional) object ID of the possesable the character is currently in
		 * @param Possessable the ID of the vehicle the character is in
		 */
		void SetPossessable(LWOOBJID Possessable) { m_Possessable = Possessable;  m_DirtyPossesor = true; }

		/**
		 * Sets the possesible type that's currently used, merely used by the shooting gallery if it's 0
		 * @param value the possesible type to set
		 */
		void SetPossessableType(ePossessionType value) { m_PossessableType = value;  m_DirtyPossesor = true; }

	private:

		/**
		 * Whether this character Possessable has changed
		 */
		bool m_DirtyPossesor = false;

		/**
		 * If the possessor is currently dismounting
		 */
		bool m_IsDismounting = false;

		/**
		 * The object ID of the vehicle the character is currently in
		 */
		LWOOBJID m_Possessable = LWOOBJID_EMPTY;

		/**
		 * Possessible type, used by the shooting gallery
		 */
		ePossessionType m_PossessableType = ePossessionType::NO_POSSESSION;

		/**
		 * The reference to the item in the possessor's inventory
		 */
		Item* m_PossesableItem = nullptr;

};