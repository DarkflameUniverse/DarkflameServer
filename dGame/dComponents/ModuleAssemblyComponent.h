#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"

/**
 * Component that belongs to an object that may be modularly built, like cars and rockets. Note that this is not the
 * same as having said items in your inventory (the subkey for this component) this component is the one that
 * renders the entity into the world.
 */
class ModuleAssemblyComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_MODULE_ASSEMBLY;

	ModuleAssemblyComponent(Entity* MSG_CHAT_INTERNAL_PLAYER_REMOVED_NOTIFICATION);
	~ModuleAssemblyComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	void Update(float deltaTime) override;

	/**
	 * Sets the subkey of this entity
	 * @param value the subkey to set
	 */
	void SetSubKey(LWOOBJID value);

	/**
	 * Returns the subkey for this entity
	 * @return the subkey for this entity
	 */
	LWOOBJID GetSubKey() const;


	/**
	 * Sets the optional parts value
	 * @param value the value to set
	 */
	void SetUseOptionalParts(bool value);

	/**
	 * Returns the optional parts value
	 * @return the value to set
	 */
	bool GetUseOptionalParts() const;

	/**
	 * Sets the assembly part lots (the subsections of this modular build)
	 * @param value the assembly part lots to set
	 */
	void SetAssemblyPartsLOTs(const std::u16string& value);

	/**
	 * Returns the assembly part lots (the subsections of this modular build)
	 * @return
	 */
	const std::u16string& GetAssemblyPartsLOTs() const;

private:

	/**
	 * The sub key is the entity that this entity is an instance of. E.g. the item in the inventory. If a car for
	 * example is to be rendered, this sub key refers to the car item that was used to build this entity.
	 */
	LWOOBJID m_SubKey;

	/**
	 * Whether to use optional parts, currently unused
	 */
	bool m_UseOptionalParts;

	/**
	 * The sub items that this entity is made of
	 */
	std::u16string m_AssemblyPartsLOTs;
};
