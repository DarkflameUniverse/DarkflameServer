// Darkflame Universe
// Copyright 2024

#include "RigidbodyPhantomPhysicsComponent.h"
#include "Entity.h"

#include "dpEntity.h"
#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"
#include "dpWorld.h"

RigidbodyPhantomPhysicsComponent::RigidbodyPhantomPhysicsComponent(Entity* parent) : PhysicsComponent(parent) {
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();
	m_Scale = m_Parent->GetDefaultScale();

	m_dpEntity = CreatePhysicsEntity(ComponentType);
	if (!m_dpEntity) return;
	m_dpEntity->SetScale(m_Scale);
	m_dpEntity->SetRotation(m_Rotation);
	m_dpEntity->SetPosition(m_Position);
	dpWorld::AddEntity(m_dpEntity);
}

void RigidbodyPhantomPhysicsComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	PhysicsComponent::Serialize(outBitStream, bIsInitialUpdate);
}

void RigidbodyPhantomPhysicsComponent::Update(const float deltaTime) {
	if (!m_dpEntity) return;

	//Process enter events
	for (const auto id : m_dpEntity->GetNewObjects()) {
		m_Parent->OnCollisionPhantom(id);
	}

	//Process exit events
	for (const auto id : m_dpEntity->GetRemovedObjects()) {
		m_Parent->OnCollisionLeavePhantom(id);
	}
}
