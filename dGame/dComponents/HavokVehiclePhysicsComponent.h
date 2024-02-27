#pragma once

#include "BitStream.h"
#include "Entity.h"
#include "PhysicsComponent.h"
#include "eReplicaComponentType.h"
#include "PositionUpdate.h"

/**
 * Physics component for vehicles.
 */
class HavokVehiclePhysicsComponent : public PhysicsComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::HAVOK_VEHICLE_PHYSICS;

	HavokVehiclePhysicsComponent(Entity* parentEntity);

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

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

	void SetRemoteInputInfo(const RemoteInputInfo&);

private:
	NiPoint3 m_Velocity;
	NiPoint3 m_AngularVelocity;

	bool m_IsOnGround;
	bool m_IsOnRail;

	float m_SoftUpdate = 0;
	uint32_t m_EndBehavior;
	RemoteInputInfo m_RemoteInputInfo;
};
