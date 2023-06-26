#pragma once
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"
#include "eReplicaComponentType.h"

class Entity;

/**
 * Component that represents entities that are a model, e.g. collectible models and BBB models.
 */
class MutableModelBehaviorComponent : public Component {
public:
	static const eReplicaComponentType ComponentType = eReplicaComponentType::MODEL_BEHAVIOR;

	MutableModelBehaviorComponent(Entity* parent);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

private:

	/**
	 * if the behavior info has changed
	 */
	bool m_DirtyModelBehaviorInfo;

	/**
	 * The number of behaviors on the model
	 */
	uint32_t m_BehaviorCount;

	/**
	 * if the models behaviors are paused
	 */
	bool m_IsPaused;

	/**
	 * if the editing info is dirty
	 */
	bool m_DirtyModelEditingInfo;

	/**
	 * The old ID of the model
	 */
	LWOOBJID m_OldObjId;

	/**
	 * The ID of the editor of the model
	 */
	LWOOBJID m_Editor;
};
