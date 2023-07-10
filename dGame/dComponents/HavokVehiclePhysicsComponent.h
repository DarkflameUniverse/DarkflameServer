#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "Component.h"
#include "eReplicaComponentType.h"

struct RemoteInputInfo {
	void operator=(const RemoteInputInfo& other) {
		m_RemoteInputX = other.m_RemoteInputX;
		m_RemoteInputY = other.m_RemoteInputY;
		m_IsPowersliding = other.m_IsPowersliding;
		m_IsModified = other.m_IsModified;
	}

	bool operator==(const RemoteInputInfo& other) {
		return m_RemoteInputX == other.m_RemoteInputX && m_RemoteInputY == other.m_RemoteInputY && m_IsPowersliding == other.m_IsPowersliding && m_IsModified == other.m_IsModified;
	}

	float m_RemoteInputX;
	float m_RemoteInputY;
	bool m_IsPowersliding;
	bool m_IsModified;
};

/**
 * Physics component for vehicles.
 */
class HavokVehiclePhysicsComponent final : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::HAVOK_VEHICLE_PHYSICS;

	HavokVehiclePhysicsComponent(Entity* parentEntity);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

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

	void SetDirtyPosition(bool val) { m_DirtyPosition = val; }
	void SetDirtyVelocity(bool val) { m_DirtyVelocity = val; }
	void SetDirtyAngularVelocity(bool val) { m_DirtyAngularVelocity = val; }
	void SetRemoteInputInfo(const RemoteInputInfo&);

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
	uint32_t m_EndBehavior;
	RemoteInputInfo m_RemoteInputInfo;
	bool m_DirtyRemoteInput;
};
