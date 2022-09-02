#ifndef CONTROLLABLEPHYSICSCOMPONENT_H
#define CONTROLLABLEPHYSICSCOMPONENT_H

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "tinyxml2.h"
#include "Component.h"
#include "dpCollisionChecks.h"
#include "PhantomPhysicsComponent.h"

class Entity;
class dpEntity;

/**
 * Handles the movement of controllable Entities, e.g. enemies and players
 */
class ControllablePhysicsComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_CONTROLLABLE_PHYSICS;

	ControllablePhysicsComponent(Entity* entity);
	~ControllablePhysicsComponent() override;

	void Update(float deltaTime) override;
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	void LoadFromXml(tinyxml2::XMLDocument* doc) override;
	void ResetFlags();
	void UpdateXml(tinyxml2::XMLDocument* doc) override;

	/**
	 * Sets the position of this entity, also ensures this update is serialized next tick.
	 * If the entity is static, this is a no-op.
	 * @param pos The position to set
	 */
	void SetPosition(const NiPoint3& pos);

	/**
	 * Returns the current position of the entity
	 * @return The current position of the entity
	 */
	const NiPoint3& GetPosition() const { return m_Position; }

	/**
	 * Sets the rotation of this entity, ensures this change is serialized next tick. If the entity is static, this is
	 * a no-op.
	 * @param rot the rotation to set
	 */
	void SetRotation(const NiQuaternion& rot);

	/**
	 * Returns the current rotation of this entity
	 * @return the current rotation of this entity
	 */
	const NiQuaternion& GetRotation() const { return m_Rotation; }

	/**
	 * Sets the current velocity of this entity, ensures that this change is serialized next tick. If the entity is
	 * marked as static this is a no-op.
	 * @param vel the velocity to set
	 */
	void SetVelocity(const NiPoint3& vel);

	/**
	 * Returns the current velocity of this entity
	 * @return the current velocity of this entity
	 */
	const NiPoint3& GetVelocity() const { return m_Velocity; }

	/**
	 * Sets the angular velocity (e.g. rotational velocity) of this entity and ensures this is serialized next tick.
	 * If the entity is marked as static this is a no-op.
	 * @param vel the angular velocity to set.
	 */
	void SetAngularVelocity(const NiPoint3& vel);

	/**
	 * Returns the current angular velocity of this entity
	 * @return the current angular velocity of this entity
	 */
	const NiPoint3& GetAngularVelocity() const { return m_AngularVelocity; }

	/**
	 * Sets the IsOnGround value, determining whether or not the entity is stuck to the ground. Note that this is mostly
	 * a client side flag as no server-side entities jump around and therefore this does not have to be updated server
	 * side.
	 * @param val whether the entity is on the ground.
	 */
	void SetIsOnGround(bool val);

	/**
	 * Returns whether or not the entity is currently on the ground
	 * @return whether the entity is currently on the ground
	 */
	const bool GetIsOnGround() const { return m_IsOnGround; }

	/**
	 * Sets the on-rail parameter, determining if a player is currently on a rail (e.g. the lamps in Ninjago).
	 * Also ensures that this change is serialized.
	 * @param val whether the player is currently on a rail
	 */
	void SetIsOnRail(bool val);

	/**
	 * Returns whether or not this entity is currently on a rail.
	 * @return whether or not this entity is currently on a rail
	 */
	const bool GetIsOnRail() const { return m_IsOnRail; }

	/**
	 * Mark the position as dirty, forcing a serialization update next tick
	 * @param val whether or not the position is dirty
	 */
	void SetDirtyPosition(bool val);

	/**
	 * Mark the velocity as dirty, forcing a serializtion update next tick
	 * @param val whether or not the velocity is dirty
	 */
	void SetDirtyVelocity(bool val);

	/**
	 * Mark the angular velocity as dirty, forcing a serialization update next tick
	 * @param val whether or not the angular velocity is dirty
	 */
	void SetDirtyAngularVelocity(bool val);

	/**
	 * Sets whether or not the entity is currently wearing a jetpack
	 * @param val whether or not the entity is currently wearing a jetpack
	 */
	void SetInJetpackMode(bool val) { m_InJetpackMode = val; }

	/**
	 * Returns whether or not the entity is currently wearing a jetpack
	 * @return whether or not the entity is currently wearing a jetpack
	 */
	const bool GetInJetpackMode() const { return m_InJetpackMode; }

	/**
	 * Sets whether or not the entity is currently flying a jetpack
	 * @param val whether or not the entity is currently flying a jetpack
	 */
	void SetJetpackFlying(bool val) { m_JetpackFlying = val; }

	/**
	 * Returns whether or not an entity is currently flying a jetpack
	 * @return whether or not an entity is currently flying a jetpack
	 */
	const bool GetJetpackFlying() const { return m_JetpackFlying; }

	/**
	 * UNUSED: necessary for serialization
	 */
	void SetJetpackBypassChecks(bool val) { m_JetpackBypassChecks = val; }

	/**
	 * UNUSUED: necessary for serialization
	 */
	const bool GetJetpackBypassChecks() const { return m_JetpackBypassChecks; }

	/**
	 * Set the jetpack effect ID
	 * @param effectID the effect to play while using the jetpack
	 */
	void SetJetpackEffectID(int effectID) { m_JetpackEffectID = effectID; }

	/**
	 * Returns the current jetpack effect ID
	 * @return the current jetpack effect ID
	 */
	const int GetJetpackEffectID() const { return m_JetpackEffectID; }

	/**
	 * Sets a speed multiplier, altering the entities speed
	 * @param value the multiplier to set
	 */
	void SetSpeedMultiplier(float value) { m_SpeedMultiplier = value; m_DirtyCheats = true; }

	/**
	 * Returns the current speed multiplier
	 * @return the current speed multiplier
	 */
	const float GetSpeedMultiplier() const { return m_SpeedMultiplier; }

	/**
	 * Sets the current gravity scale, allowing the entity to move using altered gravity
	 * @param value the gravity value to set
	 */
	void SetGravityScale(float value) { m_GravityScale = value; m_DirtyCheats = true; }

	/**
	 * Returns the current gravity scale
	 * @return the current gravity scale
	 */
	const float GetGravityScale() const { return m_GravityScale; }

	/**
	 * Sets the ignore multipliers value, allowing you to skip the serialization of speed and gravity multipliers
	 * @param value whether or not to ignore multipliers
	 */
	void SetIgnoreMultipliers(bool value) { m_IgnoreMultipliers = value; }

	/**
	 * Returns the current ignore multipliers value
	 * @return the current ignore multipliers value
	 */
	const bool GetIgnoreMultipliers() const { return m_IgnoreMultipliers; }

	/**
	 * Can make an entity static, making it unable to move around
	 * @param value whether or not the entity is static
	 */
	void SetStatic(const bool value) { m_Static = value; }

	/**
	 * Returns whether or not this entity is currently static
	 * @return whether or not this entity is currently static
	 */
	bool GetStatic() const { return m_Static; }

	/**
	 * Sets if the entity is Teleporting,
	 * @param value whether or not the entity is Is Teleporting
	 */
	void SetIsTeleporting(const bool value) { m_IsTeleporting = value; }

	/**
	 * Returns whether or not this entity is currently is teleporting
	 * @return whether or not this entity is currently is teleporting
	 */
	bool GetIsTeleporting() const { return m_IsTeleporting; }

	/**
	 * Returns the Physics entity for the component
	 * @return Physics entity for the component
	 */

	dpEntity* GetdpEntity() const { return m_dpEntity; }

	/**
	 * I store this in a vector because if I have 2 separate pickup radii being applied to the player, I dont know which one is correctly active.
	 * This method adds the pickup radius to the vector of active radii and if its larger than the current one, is applied as the new pickup radius.
	 */
	void AddPickupRadiusScale(float value);

	/**
	 * Removes the provided pickup radius scale from our list of buffs
	 * The recalculates what our pickup radius is.
	 */
	void RemovePickupRadiusScale(float value);

	/**
	 * The pickup radii of this component.
	 * @return All active radii scales for this component.
	 */
	std::vector<float> GetActivePickupRadiusScales() { return m_ActivePickupRadiusScales; };

private:
	/**
	 * The entity that owns this component
	 */
	dpEntity* m_dpEntity;

	/**
	 * Whether or not the position is dirty, forcing a serialization update of the position
	 */
	bool m_DirtyPosition;

	/**
	 * The current position of the entity
	 */
	NiPoint3 m_Position;

	/**
	 * The current rotation of the entity
	 */
	NiQuaternion m_Rotation;

	/**
	 * Whether or not the velocity is dirty, forcing a serialization of the velocity
	 */
	bool m_DirtyVelocity;

	/**
	 * The current velocity of the entity
	 */
	NiPoint3 m_Velocity;

	/**
	 * Whether or not the angular velocity is dirty, forcing a serialization
	 */
	bool m_DirtyAngularVelocity;

	/**
	 * The current angular velocity of the entity
	 */
	NiPoint3 m_AngularVelocity;

	/**
	 * Whether or not the entity is on the ground, generally unused
	 */
	bool m_IsOnGround;

	/**
	 * Whether or not the entity is on a rail, e.g. in Ninjago
	 */
	bool m_IsOnRail;

	/**
	 * Whether or not this entity has a jetpack equipped
	 */
	bool m_InJetpackMode;

	/**
	 * Whether or not this entity is currently flying a jetpack
	 */
	bool m_JetpackFlying;

	/**
	 * Bypass jetpack checks, currently unused
	 */
	bool m_JetpackBypassChecks;

	/**
	 * The effect that plays while using the jetpack
	 */
	int32_t m_JetpackEffectID;

	/**
	 * The current speed multiplier, allowing an entity to run faster
	 */
	float m_SpeedMultiplier;

	/**
	 * The current gravity scale, allowing an entity to move at an altered gravity
	 */
	float m_GravityScale;

	/**
	 * Forces a serialization of the speed multiplier and the gravity scale
	 */
	bool m_DirtyCheats;

	/**
	 * Makes it so that the speed multiplier and gravity scale are no longer serialized if false
	 */
	bool m_IgnoreMultipliers;

	/**
	 * Whether this entity is static, making it unable to move
	 */
	bool m_Static;

	/**
	 * Whether the pickup scale is dirty.
	 */
	bool m_DirtyPickupRadiusScale;

	/**
	 * The list of pickup radius scales for this entity
	 */
	std::vector<float> m_ActivePickupRadiusScales;

	/**
	 * The active pickup radius for this entity
	 */
	float m_PickupRadius;

	/**
	 * If the entity is teleporting
	 */
	bool m_IsTeleporting;
};

#endif // CONTROLLABLEPHYSICSCOMPONENT_H
