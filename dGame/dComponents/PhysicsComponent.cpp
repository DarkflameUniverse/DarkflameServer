#include "PhysicsComponent.h"

PhysicsComponent::PhysicsComponent(const LWOOBJID& parentEntityId) : Component{ parentEntityId } {
	m_Position = NiPoint3::ZERO;
	m_Rotation = NiQuaternion::IDENTITY;
	m_DirtyPosition = false;
}

void PhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	outBitStream->Write(bIsInitialUpdate || m_DirtyPosition);
	if (bIsInitialUpdate || m_DirtyPosition) {
		outBitStream->Write(m_Position.x);
		outBitStream->Write(m_Position.y);
		outBitStream->Write(m_Position.z);
		outBitStream->Write(m_Rotation.x);
		outBitStream->Write(m_Rotation.y);
		outBitStream->Write(m_Rotation.z);
		outBitStream->Write(m_Rotation.w);
		if (!bIsInitialUpdate) m_DirtyPosition = false;
	}
}
