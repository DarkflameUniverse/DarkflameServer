#pragma once

#include "Entity.h"
#include "GameMessages.h"
#include "Component.h"
#include "eCharacterVersion.h"
#include "eReplicaComponentType.h"

/**
 * Component that handles level progression and serilization.
  *
 */

class LevelProgressionComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::LEVEL_PROGRESSION;

	/**
	 * Constructor for this component
	 * @param parent parent that contains this component
	 */
	LevelProgressionComponent(Entity* parent);

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

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
	 * Gets the current Speed Base of the entity
	 * @return the current Speed Base of the entity
	 */
	const uint32_t GetSpeedBase() const { return m_SpeedBase; }

	/**
	 * Sets the Speed Base of the entity
	 * @param SpeedBase the Speed Base to set
	 */
	void SetSpeedBase(uint32_t SpeedBase) { m_SpeedBase = SpeedBase; }

	/**
	 * Gives the player rewards for the last level that they leveled up from
	 */
	void HandleLevelUp();

	/**
	 * Gets the current Character Version of the entity
	 * @return the current Character Version of the entity
	 */
	const eCharacterVersion GetCharacterVersion() const { return m_CharacterVersion; }

	/**
	 * Sets the Character Version of the entity
	 * @param CharacterVersion the Character Version to set
	 */
	void SetCharacterVersion(eCharacterVersion CharacterVersion) { m_CharacterVersion = CharacterVersion; }

	/**
	 * Set the Base Speed retroactively of the entity
	 */
	void SetRetroactiveBaseSpeed();

private:
	/**
	 * whether the level is dirty
	 */
	bool m_DirtyLevelInfo = false;

	/**
	 * Level of the entity
	 */
	uint32_t m_Level;

	/**
	 * The base speed of the entity
	 */
	float m_SpeedBase;

	/**
	 * The Character format version
	 */
	eCharacterVersion m_CharacterVersion;

};
