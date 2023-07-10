#pragma once
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"
#include "eReplicaComponentType.h"

class Entity;
enum class ePhysicsBehaviorType : int32_t;

/**
 * Component that represents entities that are a model, e.g. collectible models and BBB models.
 */
class ModelBehaviorComponent final : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::MODEL_BEHAVIOR;

	ModelBehaviorComponent(Entity* parent);

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
	void SetPosition(const NiPoint3& pos) {
		if (m_OriginalPosition == pos) return;
		m_OriginalPosition = pos;
		m_DirtyModelInfo = true;
	}

	/**
	 * Returns the original rotation of the model
	 * @return the original rotation of the model
	 */
	const NiQuaternion& GetRotation() { return m_OriginalRotation; }

	/**
	 * Sets the original rotation of the model
	 * @param rot the original rotation to set
	 */
	void SetRotation(const NiQuaternion& rot) {
		if (m_OriginalRotation == rot) return;
		m_OriginalRotation = rot;
		m_DirtyModelInfo = true;
	}

private:

	/**
	 * if the model info has changed
	 */
	bool m_DirtyModelInfo;

	/**
	 * If the model is pickable
	 */
	bool m_IsPickable;

	/**
	 * the phsyics type of the model
	 */
	ePhysicsBehaviorType m_PhysicsType;

	/**
	 * The original position of the model
	 */
	NiPoint3 m_OriginalPosition;

	/**
	 * The rotation original of the model
	 */
	NiQuaternion m_OriginalRotation;

};
