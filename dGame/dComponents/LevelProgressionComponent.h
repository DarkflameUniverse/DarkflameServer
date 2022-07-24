#pragma once

#include "Entity.h"
#include "GameMessages.h"
#include "Component.h"

/**
 * Component that handles the LUP/WBL rocket launchpad that can be interacted with to travel to WBL worlds.
  *
 */
class LevelProgressionComponent : public Component {
public:
	static const uint32_t ComponentType = eReplicaComponentType::COMPONENT_TYPE_LEVEL_PROGRESSION;

	/**
	 * Constructor for this component, builds the m_LUPWorlds vector
	 * @param parent parent that contains this component
	 */
	LevelProgressionComponent(Entity* parent);
	~LevelProgressionComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	/**
	 * Gets the current level of the entity
	 * @return the current level of the entity
	 */
	const uint32_t GetLevel() const { return m_Level; }

	/**
	 * Sets the level of the entity
	 * @param level the level to set
	 */
	void SetLevel(uint32_t level) { m_Level = level; m_DirtyLevelInfo = true; }

	/**
	 * Gives the player rewards for the last level that they leveled up from
	 */
	void HandleLevelUp();

private:
	/**
	 * whether the level is dirty
	 */
	bool m_DirtyLevelInfo = false;

	/**
	 * Level of the entity
	 */
	uint32_t m_Level;
};
