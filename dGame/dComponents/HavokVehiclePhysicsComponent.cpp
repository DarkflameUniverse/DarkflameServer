#include "HavokVehiclePhysicsComponent.h"
#include "EntityManager.h"

HavokVehiclePhysicsComponent::HavokVehiclePhysicsComponent(Entity* parent) : Component(parent) {
	m_Position = NiPoint3::ZERO;
	m_Rotation = NiQuaternion::IDENTITY;
	m_Velocity = NiPoint3::ZERO;
	m_AngularVelocity = NiPoint3::ZERO;
	m_IsOnGround = true;
	m_IsOnRail = false;
	m_DirtyPosition = true;
	m_DirtyVelocity = true;
	m_DirtyAngularVelocity = true;
	m_EndBehavior = GeneralUtils::GenerateRandomNumber<uint32_t>(0, 7);
}

HavokVehiclePhysicsComponent::~HavokVehiclePhysicsComponent() {

}

void HavokVehiclePhysicsComponent::SetPosition(const NiPoint3& pos) {
	m_Position = pos;
}

void HavokVehiclePhysicsComponent::SetRotation(const NiQuaternion& rot) {
	m_DirtyPosition = true;
	m_Rotation = rot;
}

void HavokVehiclePhysicsComponent::SetVelocity(const NiPoint3& vel) {
	m_DirtyPosition = true;
	m_Velocity = vel;
}

void HavokVehiclePhysicsComponent::SetAngularVelocity(const NiPoint3& vel) {
	m_DirtyPosition = true;
	m_AngularVelocity = vel;
}

void HavokVehiclePhysicsComponent::SetIsOnGround(bool val) {
	m_DirtyPosition = true;
	m_IsOnGround = val;
}

void HavokVehiclePhysicsComponent::SetIsOnRail(bool val) {
	m_DirtyPosition = true;
	m_IsOnRail = val;
}

void HavokVehiclePhysicsComponent::SetDirtyPosition(bool val) {
	m_DirtyPosition = val;
}

void HavokVehiclePhysicsComponent::SetDirtyVelocity(bool val) {
	m_DirtyVelocity = val;
}

void HavokVehiclePhysicsComponent::SetDirtyAngularVelocity(bool val) {
	m_DirtyAngularVelocity = val;
}

void HavokVehiclePhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
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
		outBitStream->Write<uint8_t>(m_EndBehavior);
		outBitStream->Write1();
	}

	outBitStream->Write0();
}

void HavokVehiclePhysicsComponent::Update(float deltaTime) {
	if (m_SoftUpdate > 5) {
		EntityManager::Instance()->SerializeEntity(m_ParentEntity);

		m_SoftUpdate = 0;
	} else {
		m_SoftUpdate += deltaTime;
	}
}
