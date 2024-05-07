// Darkflame Universe
// Copyright 2024

#include "RigidbodyPhantomPhysicsComponent.h"
#include "Entity.h"

#include "dpEntity.h"
#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"
#include "dpWorld.h"
#include "dpShapeBox.h"
#include "dpShapeSphere.h"
#include"EntityInfo.h"

RigidbodyPhantomPhysicsComponent::RigidbodyPhantomPhysicsComponent(Entity* parent) : PhysicsComponent(parent) {
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();
	m_Scale = m_Parent->GetDefaultScale();

	if (m_Parent->GetVar<bool>(u"create_physics")) {
		m_dpEntity = CreatePhysicsLnv(m_Scale, ComponentType);
		if (!m_dpEntity) {
			m_dpEntity = CreatePhysicsEntity(ComponentType);
			if (!m_dpEntity) return;
			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position);
			dpWorld::AddEntity(m_dpEntity);
		}
	}
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

void RigidbodyPhantomPhysicsComponent::SpawnVertices() const {
	if (!m_dpEntity) {
		LOG("No dpEntity to spawn vertices for %llu:%i", m_Parent->GetObjectID(), m_Parent->GetLOT());
		return;
	}
	PhysicsComponent::SpawnVertices(m_dpEntity);
}
