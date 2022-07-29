#pragma once

#include "Entity.h"
#include "GameMessages.h"
#include "Component.h"

/**
 * Component that handles level progression and serilization.
  *
 */
class LevelProgressionComponent : public Component {
public:
	static const uint32_t ComponentType = eReplicaComponentType::COMPONENT_TYPE_LEVEL_PROGRESSION;

	/**
	 * Constructor for this component
	 * @param parent parent that contains this component
	 */
	LevelProgressionComponent(Entity* parent);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Save data from this componennt to character XML
	 * @param doc the document to write data to
	 */
	void UpdateXml(tinyxml2::XMLDocument* doc) override;

	/**
	 * Load base data for this component from character XML
	 * @param doc the document to read data from
	 */
	void LoadFromXml(tinyxml2::XMLDocument* doc) override;

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
