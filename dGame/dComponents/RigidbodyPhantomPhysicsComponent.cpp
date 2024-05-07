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

	if (m_Parent->GetVar<bool>(u"create_physics") || m_Parent->GetLOT() == 11386) {
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

void RigidbodyPhantomPhysicsComponent::SpawnVertices() {
	if (!m_dpEntity) return;

	LOG("%llu", m_Parent->GetObjectID());
	auto box = dynamic_cast<dpShapeBox*>(m_dpEntity->GetShape());
	if (box) {
		for (auto vert : box->GetVertices()) {
			LOG("%f, %f, %f", vert.x, vert.y, vert.z);

			EntityInfo info;
			info.lot = 33;
			info.pos = vert;
			info.spawner = nullptr;
			info.spawnerID = m_Parent->GetObjectID();
			info.spawnerNodeID = 0;

			Entity* newEntity = Game::entityManager->CreateEntity(info, nullptr);
			Game::entityManager->ConstructEntity(newEntity);
		}
	}
	auto sphere = dynamic_cast<dpShapeSphere*>(m_dpEntity->GetShape());
	if (sphere) {
		auto [x, y, z] = m_dpEntity->GetPosition();
		float plusX = x + sphere->GetRadius();
		float minusX = x - sphere->GetRadius();
		float plusY = y + sphere->GetRadius();
		float minusY = y - sphere->GetRadius();
		float plusZ = z + sphere->GetRadius();
		float minusZ = z - sphere->GetRadius();

		auto radius = sphere->GetRadius();
		LOG("Radius: %f", radius);
		LOG("Verts %f %f %f", plusX, plusY, plusZ);
		LOG("Verts %f %f %f", minusX, minusY, minusZ);
		EntityInfo info;
		info.lot = 33;
		info.spawner = nullptr;
		info.spawnerID = m_Parent->GetObjectID();
		info.spawnerNodeID = 0;

		info.pos = { x, plusY, z };
		Entity* newEntity = Game::entityManager->CreateEntity(info, nullptr);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = { x, minusY, z };
		newEntity = Game::entityManager->CreateEntity(info, nullptr);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = { plusX, y, z };
		newEntity = Game::entityManager->CreateEntity(info, nullptr);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = { minusX, y, z };
		newEntity = Game::entityManager->CreateEntity(info, nullptr);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = { x, y, plusZ };
		newEntity = Game::entityManager->CreateEntity(info, nullptr);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = { x, y, minusZ };
		newEntity = Game::entityManager->CreateEntity(info, nullptr);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = { x, y, z };
		newEntity = Game::entityManager->CreateEntity(info, nullptr);
		Game::entityManager->ConstructEntity(newEntity);
	}
}
