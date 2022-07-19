#pragma once
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"

class Entity;

/**
 * Component that represents entities that are a model, e.g. collectible models and BBB models.
 */
class ModelComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_MODEL;

	ModelComponent(Entity* parent);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Returns the original position of the model
	 * @return the original position of the model
	 */
	const NiPoint3& GetPosition() { return m_OriginalPosition; }

	/**
	 * Sets the original position of the model
	 * @param pos the original position to set
	 */
	void SetPosition(const NiPoint3& pos) { m_OriginalPosition = pos; }

	/**
	 * Returns the original rotation of the model
	 * @return the original rotation of the model
	 */
	const NiQuaternion& GetRotation() { return m_OriginalRotation; }

	/**
	 * Sets the original rotation of the model
	 * @param rot the original rotation to set
	 */
	void SetRotation(const NiQuaternion& rot) { m_OriginalRotation = rot; }

private:

	/**
	 * The original position of the model
	 */
	NiPoint3 m_OriginalPosition;

	/**
	 * The rotation original of the model
	 */
	NiQuaternion m_OriginalRotation;

	/**
	 * The ID of the user that made the model
	 */
	LWOOBJID m_userModelID;
};
