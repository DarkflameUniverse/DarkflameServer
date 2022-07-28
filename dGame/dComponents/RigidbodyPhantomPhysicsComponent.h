/*
 * Darkflame Universe
 * Copyright 2019
 */

#ifndef RIGIDBODYPHANTOMPHYSICS_H
#define RIGIDBODYPHANTOMPHYSICS_H

#include "BitStream.h"
#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"

 /**
  * Component that handles rigid bodies that can be interacted with, mostly client-side rendered. An example is the
  * bananas that fall from trees in GF.
  */
class RigidbodyPhantomPhysicsComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_PHANTOM_PHYSICS;

	RigidbodyPhantomPhysicsComponent(Entity* parent);
	~RigidbodyPhantomPhysicsComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Returns the position of this entity
	 * @return the position of this entity
	 */
	NiPoint3& GetPosition() { return m_Position; }

	/**
	 * Sets the position of this entity
	 * @param pos the position to set
	 */
	void SetPosition(const NiPoint3& pos) { m_Position = pos; m_IsDirty = true; }

	/**
	 * Returns the rotation of this entity
	 * @return the rotation of this entity
	 */
	NiQuaternion& GetRotation() { return m_Rotation; }

	/**
	 * Sets the rotation for this entity
	 * @param rot the rotation to tset
	 */
	void SetRotation(const NiQuaternion& rot) { m_Rotation = rot; m_IsDirty = true; }

private:

	/**
	 * The position of this entity
	 */
	NiPoint3 m_Position;

	/**
	 * The rotation of this entity
	 */
	NiQuaternion m_Rotation;

	/**
	 * Whether or not the component should be serialized
	 */
	bool m_IsDirty;
};

#endif // RIGIDBODYPHANTOMPHYSICS_H
