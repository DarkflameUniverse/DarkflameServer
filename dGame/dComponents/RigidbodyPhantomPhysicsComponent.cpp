/*
 * Darkflame Universe
 * Copyright 2019
 */

#include "RigidbodyPhantomPhysicsComponent.h"
#include "Entity.h"

RigidbodyPhantomPhysicsComponent::RigidbodyPhantomPhysicsComponent(Entity* parent) : Component(parent) {
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();
	m_IsDirty = true;
}

RigidbodyPhantomPhysicsComponent::~RigidbodyPhantomPhysicsComponent() {
}

void RigidbodyPhantomPhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_IsDirty || bIsInitialUpdate);
	if (m_IsDirty || bIsInitialUpdate) {
		outBitStream->Write(m_Position.x);
		outBitStream->Write(m_Position.y);
		outBitStream->Write(m_Position.z);

		outBitStream->Write(m_Rotation.x);
		outBitStream->Write(m_Rotation.y);
		outBitStream->Write(m_Rotation.z);
		outBitStream->Write(m_Rotation.w);

		m_IsDirty = false;
	}
}
