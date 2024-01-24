/*
 * Darkflame Universe
 * Copyright 2023
 */

#include "RigidbodyPhantomPhysicsComponent.h"
#include "Entity.h"

RigidbodyPhantomPhysicsComponent::RigidbodyPhantomPhysicsComponent(const LWOOBJID& parentEntityId) : PhysicsComponent{ parentEntityId } {
	auto* const parentEntity = Game::entityManager->GetEntity(m_Parent);
	
	m_Position = parentEntity->GetDefaultPosition();
	m_Rotation = parentEntity->GetDefaultRotation();
}

void RigidbodyPhantomPhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	PhysicsComponent::Serialize(outBitStream, bIsInitialUpdate);
}
