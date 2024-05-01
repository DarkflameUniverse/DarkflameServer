// Darkflame Universe
// Copyright 2024

#include "RigidbodyPhantomPhysicsComponent.h"
#include "Entity.h"

RigidbodyPhantomPhysicsComponent::RigidbodyPhantomPhysicsComponent(Entity* parent) : PhysicsComponent(parent) {
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();
}

void RigidbodyPhantomPhysicsComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	PhysicsComponent::Serialize(outBitStream, bIsInitialUpdate);
}
