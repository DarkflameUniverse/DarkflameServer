/*
 * Darkflame Universe
 * Copyright 2018
 */

#pragma once

#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "BitStream.h"
#include <vector>
#include "CppScripts.h"
#include "InvalidScript.h"
#include "Component.h"

class LDFBaseData;
class Entity;
class dpEntity;

/**
 * Allows the creation of phantom physics for an entity: a physics object that is generally invisible but can be
 * collided with using various shapes to detect collision on certain points in the map. Generally these are used to
 * trigger gameplay events, for example the bus in Avant Gardens that moves around when the player touches its physics
 * body. Optionally this object can also have effects, like the fans in AG.
 */
class PhantomPhysicsComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_PHANTOM_PHYSICS;

	PhantomPhysicsComponent(Entity* parent);
	~PhantomPhysicsComponent() override;
	void Update(float deltaTime) override;
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	void ResetFlags();

	/**
	 * Creates the physics shape for this entity based on LDF data
	 */
	void CreatePhysics();

	/**
	 * Sets the direction this physics object is pointed at
	 * @param pos the direction to set
	 */
	void SetDirection(const NiPoint3& pos);

	/**
	 * Returns the direction this physics object is pointed at
	 * @return the direction this physics object is pointed at
	 */
	const NiPoint3& GetDirection() const { return m_Direction; }

	/**
	 * Returns the multiplier by which the direction coordinates are multiplied
	 * @return the multiplier by which the direction coordinates are multiplied
	 */
	float GetDirectionalMultiplier() const { return m_DirectionalMultiplier; }

	/**
	 * Sets the multiplier by which direction coordinates are multiplied
	 * @param mul the multiplier to set
	 */
	void SetDirectionalMultiplier(float mul);

	/**
	 * Returns whether or not there's currently an effect active
	 * @return true if there's an effect active, false otherwise
	 */
	bool GetPhysicsEffectActive() const { return m_IsPhysicsEffectActive; }

	/**
	 * Sets whether or not there's a physics effect active
	 * @param val whether or not there's an effect active
	 */
	void SetPhysicsEffectActive(bool val) { m_IsPhysicsEffectActive = val; m_EffectInfoDirty = true; }

	/**
	 * Returns the position of this physics object
	 * @return the position of this physics object
	 */
	const NiPoint3& GetPosition() const { return m_Position; }

	/**
	 * Sets the position of this physics object
	 * @param pos the position to set
	 */
	void SetPosition(const NiPoint3& pos);

	/**
	 * Returns the rotation of this physics object
	 * @return the rotation of this physics object
	 */
	const NiQuaternion& GetRotation() const { return m_Rotation; }

	/**
	 * Sets the rotation of this physics object
	 * @param rot the rotation to set
	 */
	void SetRotation(const NiQuaternion& rot);

	/**
	 * Returns the effect that's currently active, defaults to 0
	 * @return the effect that's currently active
	 */
	uint32_t GetEffectType() const { return m_EffectType; }

	/**
	 * Sets the effect that's currently active
	 * @param type the effect to set
	 */
	void SetEffectType(uint32_t type);

	/**
	 * Returns the Physics entity for the component
	 * @return Physics entity for the component
	 */

	dpEntity* GetdpEntity() const { return m_dpEntity; }

	/**
	 * Spawns an object at each of the vertices for debugging purposes
	 */
	void SpawnVertices();

	/**
	 * Legacy stuff no clue what this does
	 */
	void SetMin(uint32_t min);

	/**
	 * Legacy stuff no clue what this does
	 */
	void SetMax(uint32_t max);

private:

	/**
	 * The position of the physics object
	 */
	NiPoint3 m_Position;

	/**
	 * The rotation of the physics object
	 */
	NiQuaternion m_Rotation;

	/**
	 * A scale to apply to the size of the physics object
	 */
	float m_Scale;

	/**
	 * Whether or not the position has changed and needs to be serialized
	 */
	bool m_PositionInfoDirty;

	/**
	 * Whether or not the effect has changed and needs to be serialized
	 */
	bool m_EffectInfoDirty;

	/**
	 * Whether or not there's currently a physics effect active
	 */
	bool m_IsPhysicsEffectActive;

	/**
	 * The physics effect that's currently active, defaults to 0
	 */
	uint32_t m_EffectType;

	/**
	 * A scaling multiplier to add to the directional vector
	 */
	float m_DirectionalMultiplier;

	bool m_MinMax;
	uint32_t m_Min;
	uint32_t m_Max;

	/**
	 * Whether or not this physics object is pointed in some direction
	 */
	bool m_IsDirectional;

	/**
	 * The direction this physics object is pointed in, if any
	 */
	NiPoint3 m_Direction;

	/**
	 * The parent entity of this component
	 */
	dpEntity* m_dpEntity;

	/**
	 * Whether or not the physics object has been created yet
	 */
	bool m_HasCreatedPhysics = false;

	/**
	 * Whether or not this physics object represents an object that updates the respawn pos of an entity that crosses it
	 */
	bool m_IsRespawnVolume = false;

	/**
	 * If this is a respawn volume, the exact position an entity will respawn
	 */
	NiPoint3 m_RespawnPos;

	/**
	 * If this is a respawn volume, the exact rotation an entity will respawn
	 */
	NiQuaternion m_RespawnRot;
};
