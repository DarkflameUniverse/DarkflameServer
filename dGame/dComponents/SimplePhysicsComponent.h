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
#include "PhysicsComponent.h"
#include "eReplicaComponentType.h"

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
class SimplePhysicsComponent : public PhysicsComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SIMPLE_PHYSICS;

	SimplePhysicsComponent(Entity* parent, int32_t componentID);
	~SimplePhysicsComponent() override;

	void Update(const float deltaTime) override;

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

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
	bool OnGetObjectReportInfo(GameMessages::GameMsg& msg);

	/**
	 * The current velocity of the entity
	 */
	NiPoint3 m_Velocity = NiPoint3Constant::ZERO;

	/**
	 * The current angular velocity of the entity
	 */
	NiPoint3 m_AngularVelocity = NiPoint3Constant::ZERO;

	/**
	 * Whether or not the velocity has changed
	 */
	bool m_DirtyVelocity = true;

	/**
	 * The current physics motion state
	 */
	uint32_t m_PhysicsMotionState = 5;

	bool m_DirtyPhysicsMotionState = true;

	/**
	 * Whether or not the entity is climbable
	 */
	eClimbableType m_ClimbableType = eClimbableType::CLIMBABLE_TYPE_NOT;
};

#endif // SIMPLEPHYSICSCOMPONENT_H
