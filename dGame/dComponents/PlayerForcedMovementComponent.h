#pragma once

#include "Entity.h"
#include "Component.h"

/**
 * Component that handles player forced movement
  *
 */
class PlayerForcedMovementComponent : public Component {
public:
	static const uint32_t ComponentType = eReplicaComponentType::COMPONENT_TYPE_PLAYER_FORCED_MOVEMENT;

	/**
	 * Constructor for this component
	 * @param parent parent that contains this component
	 */
	PlayerForcedMovementComponent(Entity* parent);
	~PlayerForcedMovementComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * @brief Set the Player On Rail object
	 *
	 * @param value if the player is on a rail
	 */
	void SetPlayerOnRail(bool value) { m_PlayerOnRail = value; m_DirtyInfo = true; }

	/**
	 * @brief Set the Show Billboard object
	 *
	 * @param value if the billboard should be shown
	 */
	void SetShowBillboard(bool value) { m_ShowBillboard = value; m_DirtyInfo = true; }

	/**
	 * @brief Get the Player On Rail object
	 *
	 * @return true
	 * @return false
	 */

	 /**
	  * @brief Get the Player On Rail object
	  *
	  * @return true
	  * @return false
	  */
	bool GetPlayerOnRail() { return m_PlayerOnRail; }
	bool GetShowBillboard() { return m_ShowBillboard; }

private:
	/**
	 * whether the info is dirty
	 */
	bool m_DirtyInfo = false;

	/**
	 * whether the player is on a rail
	 */
	bool m_PlayerOnRail = false;

	/**
	 * whether the billboard should be showing
	 */
	bool m_ShowBillboard = false;

};
