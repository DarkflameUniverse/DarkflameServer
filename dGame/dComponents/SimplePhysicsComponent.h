/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef SIMPLEPHYSICSCOMPONENT_H
#define SIMPLEPHYSICSCOMPONENT_H

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"

class Entity;

enum class eClimbableType : int32_t {
	CLIMBABLE_TYPE_NOT = 0,
	CLIMBABLE_TYPE_LADDER,
	CLIMBABLE_TYPE_WALL,
	CLIMBABLE_TYPE_WALL_STICK
};


/**
 * Component that serializes locations of entities to the client
 */
class SimplePhysicsComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_SIMPLE_PHYSICS;

	SimplePhysicsComponent(uint32_t componentID, Entity* parent);
	~SimplePhysicsComponent() override;

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
	 * Sets the rotation of this entity
	 * @param rot
	 */
	void SetRotation(const NiQuaternion& rot) { m_Rotation = rot; m_IsDirty = true; }

	/**
	 * Returns the velocity of this entity
	 * @return the velocity of this entity
	 */
	const NiPoint3& GetVelocity() { return m_Velocity; }

	/**
	 * Sets the velocity of this entity
	 * @param value the velocity to set
	 */
	void SetVelocity(const NiPoint3& value) { m_Velocity = value; m_DirtyVelocity = true; }

	/**
	 * Returns the angular velocity of this entity
	 * @return the angular velocity of this entity
	 */
	const NiPoint3& GetAngularVelocity() { return m_AngularVelocity; }

	/**
	 * Sets the angular velocity of this entity
	 * @param value the angular velocity to set
	 */
	void SetAngularVelocity(const NiPoint3& value) { m_AngularVelocity = value; m_DirtyVelocity = true; }

	/**
	 * Returns the physics motion state
	 * @return the physics motion state
	 */
	uint32_t GetPhysicsMotionState() const;

	/**
	 * Sets the physics motion state
	 * @param value the motion state to set
	 */
	void SetPhysicsMotionState(uint32_t value);

	/**
	 * Returns the ClimbableType of this entity
	 * @return the ClimbableType of this entity
	 */
	const eClimbableType& GetClimabbleType() { return m_ClimbableType; }

	/**
	 * Sets the ClimbableType of this entity
	 * @param value the ClimbableType to set
	 */
	void SetClimbableType(const eClimbableType& value) { m_ClimbableType = value; }

private:

	/**
	 * The current position of the entity
	 */
	NiPoint3 m_Position = NiPoint3::ZERO;

	/**
	 * The current rotation of the entity
	 */
	NiQuaternion m_Rotation = NiQuaternion::IDENTITY;

	/**
	 * The current velocity of the entity
	 */
	NiPoint3 m_Velocity = NiPoint3::ZERO;

	/**
	 * The current angular velocity of the entity
	 */
	NiPoint3 m_AngularVelocity = NiPoint3::ZERO;

	/**
	 * Whether or not the velocity has changed
	 */
	bool m_DirtyVelocity = true;

	/**
	 * Whether or not the position has changed
	 */
	bool m_IsDirty = true;

	/**
	 * The current physics motion state
	 */
	uint32_t m_PhysicsMotionState = 0;

	/**
	 * Whether or not the entity is climbable
	 */
	eClimbableType m_ClimbableType = eClimbableType::CLIMBABLE_TYPE_NOT;
};

#endif // SIMPLEPHYSICSCOMPONENT_H
