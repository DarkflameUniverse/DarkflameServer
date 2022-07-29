#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"

/**
 * Physics component for vehicles.
 */
class VehiclePhysicsComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_VEHICLE_PHYSICS;

	VehiclePhysicsComponent(Entity* parentEntity);
	~VehiclePhysicsComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	void Update(float deltaTime) override;

	/**
	 * Sets the position
	 * @param pos the new position
	 */
	void SetPosition(const NiPoint3& pos);

	/**
	 * Gets the position
	 * @return the position
	 */
	const NiPoint3& GetPosition() const { return m_Position; }

	/**
	 * Sets the rotation
	 * @param rot the new rotation
	 */
	void SetRotation(const NiQuaternion& rot);

	/**
	 * Gets the rotation
	 * @return the rotation
	 */
	const NiQuaternion& GetRotation() const { return m_Rotation; }

	/**
	 * Sets the velocity
	 * @param vel the new velocity
	 */
	void SetVelocity(const NiPoint3& vel);

	/**
	 * Gets the velocity
	 * @return the velocity
	 */
	const NiPoint3& GetVelocity() const { return m_Velocity; }

	/**
	 * Sets the angular velocity
	 * @param vel the new angular velocity
	 */
	void SetAngularVelocity(const NiPoint3& vel);

	/**
	 * Gets the angular velocity
	 * @return the angular velocity
	 */
	const NiPoint3& GetAngularVelocity() const { return m_AngularVelocity; }

	/**
	 * Sets whether the vehicle is on the ground
	 * @param val whether the vehicle is on the ground
	 */
	void SetIsOnGround(bool val);

	/**
	 * Gets whether the vehicle is on the ground
	 * @return whether the vehicle is on the ground
	 */
	const bool GetIsOnGround() const { return m_IsOnGround; }

	/**
	 * Gets whether the vehicle is on rail
	 * @return whether the vehicle is on rail
	 */
	void SetIsOnRail(bool val);

	/**
	 * Gets whether the vehicle is on rail
	 * @return whether the vehicle is on rail
	 */
	const bool GetIsOnRail() const { return m_IsOnRail; }

	void SetDirtyPosition(bool val);
	void SetDirtyVelocity(bool val);
	void SetDirtyAngularVelocity(bool val);

private:
	bool m_DirtyPosition;
	NiPoint3 m_Position;
	NiQuaternion m_Rotation;

	bool m_DirtyVelocity;
	NiPoint3 m_Velocity;

	bool m_DirtyAngularVelocity;
	NiPoint3 m_AngularVelocity;
	bool m_IsOnGround;
	bool m_IsOnRail;

	float m_SoftUpdate = 0;
};
