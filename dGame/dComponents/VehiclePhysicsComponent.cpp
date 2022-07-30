#include "VehiclePhysicsComponent.h"
#include "EntityManager.h"

VehiclePhysicsComponent::VehiclePhysicsComponent(Entity* parent) : Component(parent) {
	m_Position = NiPoint3::ZERO;
	m_Rotation = NiQuaternion::IDENTITY;
	m_Velocity = NiPoint3::ZERO;
	m_AngularVelocity = NiPoint3::ZERO;
	m_IsOnGround = true;
	m_IsOnRail = false;
	m_DirtyPosition = true;
	m_DirtyVelocity = true;
	m_DirtyAngularVelocity = true;
}

VehiclePhysicsComponent::~VehiclePhysicsComponent() {

}

void VehiclePhysicsComponent::SetPosition(const NiPoint3& pos) {
	m_Position = pos;
}

void VehiclePhysicsComponent::SetRotation(const NiQuaternion& rot) {
	m_DirtyPosition = true;
	m_Rotation = rot;
}

void VehiclePhysicsComponent::SetVelocity(const NiPoint3& vel) {
	m_DirtyPosition = true;
	m_Velocity = vel;
}

void VehiclePhysicsComponent::SetAngularVelocity(const NiPoint3& vel) {
	m_DirtyPosition = true;
	m_AngularVelocity = vel;
}

void VehiclePhysicsComponent::SetIsOnGround(bool val) {
	m_DirtyPosition = true;
	m_IsOnGround = val;
}

void VehiclePhysicsComponent::SetIsOnRail(bool val) {
	m_DirtyPosition = true;
	m_IsOnRail = val;
}

void VehiclePhysicsComponent::SetDirtyPosition(bool val) {
	m_DirtyPosition = val;
}

void VehiclePhysicsComponent::SetDirtyVelocity(bool val) {
	m_DirtyVelocity = val;
}

void VehiclePhysicsComponent::SetDirtyAngularVelocity(bool val) {
	m_DirtyAngularVelocity = val;
}

void VehiclePhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(bIsInitialUpdate || m_DirtyPosition);

	if (bIsInitialUpdate || m_DirtyPosition) {
		outBitStream->Write(m_Position);

		outBitStream->Write(m_Rotation);

		outBitStream->Write(m_IsOnGround);
		outBitStream->Write(m_IsOnRail);

		outBitStream->Write(bIsInitialUpdate || m_DirtyVelocity);

		if (bIsInitialUpdate || m_DirtyVelocity) {
			outBitStream->Write(m_Velocity);
		}

		outBitStream->Write(bIsInitialUpdate || m_DirtyAngularVelocity);

		if (bIsInitialUpdate || m_DirtyAngularVelocity) {
			outBitStream->Write(m_AngularVelocity);
		}

		outBitStream->Write0();

		outBitStream->Write0();

		outBitStream->Write(0.0f);

		if (!bIsInitialUpdate) {
			outBitStream->Write0();
		}
	}

	if (bIsInitialUpdate) {
		outBitStream->Write<uint8_t>(5);
		outBitStream->Write1();
	}

	outBitStream->Write0();
}

void VehiclePhysicsComponent::Update(float deltaTime) {
	if (m_SoftUpdate > 5) {
		EntityManager::Instance()->SerializeEntity(m_Parent);

		m_SoftUpdate = 0;
	} else {
		m_SoftUpdate += deltaTime;
	}
}
