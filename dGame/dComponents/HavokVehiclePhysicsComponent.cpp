#include "HavokVehiclePhysicsComponent.h"
#include "EntityManager.h"

HavokVehiclePhysicsComponent::HavokVehiclePhysicsComponent(Entity* parent) : PhysicsComponent(parent) {
	m_Velocity = NiPoint3Constant::ZERO;
	m_AngularVelocity = NiPoint3Constant::ZERO;
	m_IsOnGround = true;
	m_IsOnRail = false;
	m_DirtyPosition = true;
	m_DirtyVelocity = true;
	m_DirtyAngularVelocity = true;
	m_EndBehavior = GeneralUtils::GenerateRandomNumber<uint32_t>(0, 7);
}

void HavokVehiclePhysicsComponent::SetVelocity(const NiPoint3& vel) {
	if (vel == m_Velocity) return;
	m_DirtyPosition = true;
	m_Velocity = vel;
}

void HavokVehiclePhysicsComponent::SetAngularVelocity(const NiPoint3& vel) {
	if (vel == m_AngularVelocity) return;
	m_DirtyPosition = true;
	m_AngularVelocity = vel;
}

void HavokVehiclePhysicsComponent::SetIsOnGround(bool val) {
	if (val == m_IsOnGround) return;
	m_DirtyPosition = true;
	m_IsOnGround = val;
}

void HavokVehiclePhysicsComponent::SetIsOnRail(bool val) {
	if (val == m_IsOnRail) return;
	m_DirtyPosition = true;
	m_IsOnRail = val;
}

void HavokVehiclePhysicsComponent::SetRemoteInputInfo(const RemoteInputInfo& remoteInputInfo) {
	if (m_RemoteInputInfo == remoteInputInfo) return;
	this->m_RemoteInputInfo = remoteInputInfo;
	m_DirtyRemoteInput = true;
}

void HavokVehiclePhysicsComponent::SetDirtyVelocity(bool val) {
	m_DirtyVelocity = val;
}

void HavokVehiclePhysicsComponent::SetDirtyAngularVelocity(bool val) {
	m_DirtyAngularVelocity = val;
}

void HavokVehiclePhysicsComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(bIsInitialUpdate || m_DirtyPosition);

	if (bIsInitialUpdate || m_DirtyPosition) {
		m_DirtyPosition = false;
		outBitStream.Write(m_Position.x);
		outBitStream.Write(m_Position.y);
		outBitStream.Write(m_Position.z);

		outBitStream.Write(m_Rotation.x);
		outBitStream.Write(m_Rotation.y);
		outBitStream.Write(m_Rotation.z);
		outBitStream.Write(m_Rotation.w);

		outBitStream.Write(m_IsOnGround);
		outBitStream.Write(m_IsOnRail);

		outBitStream.Write(bIsInitialUpdate || m_DirtyVelocity);

		if (bIsInitialUpdate || m_DirtyVelocity) {
			outBitStream.Write(m_Velocity.x);
			outBitStream.Write(m_Velocity.y);
			outBitStream.Write(m_Velocity.z);
			m_DirtyVelocity = false;
		}

		outBitStream.Write(bIsInitialUpdate || m_DirtyAngularVelocity);

		if (bIsInitialUpdate || m_DirtyAngularVelocity) {
			outBitStream.Write(m_AngularVelocity.x);
			outBitStream.Write(m_AngularVelocity.y);
			outBitStream.Write(m_AngularVelocity.z);
			m_DirtyAngularVelocity = false;
		}

		outBitStream.Write0(); // local_space_info. TODO: Implement this

		outBitStream.Write(m_DirtyRemoteInput || bIsInitialUpdate); // remote_input_info
		if (m_DirtyRemoteInput || bIsInitialUpdate) {
			outBitStream.Write(m_RemoteInputInfo.m_RemoteInputX);
			outBitStream.Write(m_RemoteInputInfo.m_RemoteInputY);
			outBitStream.Write(m_RemoteInputInfo.m_IsPowersliding);
			outBitStream.Write(m_RemoteInputInfo.m_IsModified);
			m_DirtyRemoteInput = false;
		}

		outBitStream.Write(125.0f); // remote_input_ping TODO: Figure out how this should be calculated as it seems to be constant through the whole race.

		if (!bIsInitialUpdate) {
			outBitStream.Write0();
		}
	}

	if (bIsInitialUpdate) {
		outBitStream.Write<uint8_t>(m_EndBehavior);
		outBitStream.Write1(); // is input locked?
	}

	outBitStream.Write0();
}

void HavokVehiclePhysicsComponent::Update(float deltaTime) {
	if (m_SoftUpdate > 5) {
		Game::entityManager->SerializeEntity(m_Parent);
		m_SoftUpdate = 0;
	} else {
		m_SoftUpdate += deltaTime;
	}
}
