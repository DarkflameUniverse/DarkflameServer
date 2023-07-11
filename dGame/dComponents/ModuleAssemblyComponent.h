#ifndef __MODULEASSEMBLYCOMPONENT__H__
#define __MODULEASSEMBLYCOMPONENT__H__

#pragma once

#include "Component.h"
#include "eReplicaComponentType.h"

namespace RakNet {
	class BitStream;
};

/**
 * Component that belongs to an object that may be modularly built, like cars and rockets. Note that this is not the
 * same as having said items in your inventory (the subkey for this component) this component is the one that
 * renders the entity into the world.
 */
class ModuleAssemblyComponent final : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::MODULE_ASSEMBLY;

	ModuleAssemblyComponent(Entity* parent);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Sets the subkey of this entity
	 * @param value the subkey to set
	 */
	void SetSubKey(const LWOOBJID& value) { m_SubKey = value; };

	/**
	 * Returns the subkey for this entity
	 * @return the subkey for this entity
	 */
	LWOOBJID GetSubKey() const { return m_SubKey; };

	/**
	 * Sets the optional parts value
	 * @param value the value to set
	 */
	void SetUseOptionalParts(bool value) { m_UseOptionalParts = value; };

	/**
	 * Returns the optional parts value
	 * @return the value to set
	 */
	bool GetUseOptionalParts() const { return m_UseOptionalParts; };

	/**
	 * Sets the assembly part lots (the subsections of this modular build)
	 * @param value the assembly part lots to set
	 */
	void SetAssemblyPartsLOTs(const std::u16string& value);

	/**
	 * Returns the assembly part lots (the subsections of this modular build)
	 * @return
	 */
	const std::u16string& GetAssemblyPartsLOTs() const { return m_AssemblyPartsLOTs; };

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

#endif  //!__MODULEASSEMBLYCOMPONENT__H__
