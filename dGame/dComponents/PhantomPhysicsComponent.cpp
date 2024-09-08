/*
 * Darkflame Universe
 * Copyright 2018
 */

#include <sstream>
#include <iostream>

#include "PhantomPhysicsComponent.h"
#include "Game.h"
#include "LDFFormat.h"
#include "Logger.h"
#include "Entity.h"
#include "EntityManager.h"
#include "ControllablePhysicsComponent.h"
#include "GameMessages.h"
#include "ePhysicsEffectType.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"
#include "dServer.h"
#include "EntityInfo.h"

#include "dpWorld.h"
#include "dpEntity.h"
#include "dpShapeBox.h"
#include "dpShapeSphere.h"

PhantomPhysicsComponent::PhantomPhysicsComponent(Entity* parent) : PhysicsComponent(parent) {
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();
	m_Scale = m_Parent->GetDefaultScale();
	m_dpEntity = nullptr;

	m_EffectInfoDirty = false;

	m_IsPhysicsEffectActive = false;
	m_EffectType = ePhysicsEffectType::PUSH;
	m_DirectionalMultiplier = 0.0f;

	m_MinMax = false;
	m_Min = 0;
	m_Max = 1;

	m_IsDirectional = false;
	m_Direction = NiPoint3(); // * m_DirectionalMultiplier

	if (m_Parent->GetVar<bool>(u"create_physics")) {
		m_dpEntity = CreatePhysicsLnv(m_Scale, ComponentType);
	}

	if (m_Parent->GetVar<bool>(u"respawnVol")) {
		m_IsRespawnVolume = true;
	}

	if (m_IsRespawnVolume) {
		{
			auto respawnString = std::stringstream(m_Parent->GetVarAsString(u"rspPos"));

			std::string segment;
			std::vector<std::string> seglist;

			while (std::getline(respawnString, segment, '\x1f')) {
				seglist.push_back(segment);
			}

			m_RespawnPos = NiPoint3(std::stof(seglist[0]), std::stof(seglist[1]), std::stof(seglist[2]));
		}

		{
			auto respawnString = std::stringstream(m_Parent->GetVarAsString(u"rspRot"));

			std::string segment;
			std::vector<std::string> seglist;

			while (std::getline(respawnString, segment, '\x1f')) {
				seglist.push_back(segment);
			}

			m_RespawnRot = NiQuaternion(std::stof(seglist[0]), std::stof(seglist[1]), std::stof(seglist[2]), std::stof(seglist[3]));
		}
	}

	// HF - RespawnPoints. Legacy respawn entity.
	if (m_Parent->GetLOT() == 4945) {
		m_IsRespawnVolume = true;
		m_RespawnPos = m_Position;
		m_RespawnRot = m_Rotation;
	}

	if (!m_dpEntity) {
		m_dpEntity = CreatePhysicsEntity(ComponentType);
		if (!m_dpEntity) return;
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position);
		dpWorld::AddEntity(m_dpEntity);
	}
}

PhantomPhysicsComponent::~PhantomPhysicsComponent() {
	if (m_dpEntity) {
		dpWorld::RemoveEntity(m_dpEntity);
	}
}

void PhantomPhysicsComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	PhysicsComponent::Serialize(outBitStream, bIsInitialUpdate);

	outBitStream.Write(m_EffectInfoDirty || bIsInitialUpdate);
	if (m_EffectInfoDirty || bIsInitialUpdate) {
		outBitStream.Write(m_IsPhysicsEffectActive);

		if (m_IsPhysicsEffectActive) {
			outBitStream.Write(m_EffectType);
			outBitStream.Write(m_DirectionalMultiplier);

			// forgive me father for i have sinned
			outBitStream.Write0();
			//outBitStream.Write(m_MinMax);
			//if (m_MinMax) {
				//outBitStream.Write(m_Min);
				//outBitStream.Write(m_Max);
			//}

			outBitStream.Write(m_IsDirectional);
			if (m_IsDirectional) {
				outBitStream.Write(m_Direction.x);
				outBitStream.Write(m_Direction.y);
				outBitStream.Write(m_Direction.z);
			}
		}

		m_EffectInfoDirty = false;
	}
}

// Even if we were to implement Friction server side,
// it also defaults to 1.0f in the last argument, so we dont need two functions to do the same thing.
void ApplyCollisionEffect(const LWOOBJID& target, const ePhysicsEffectType effectType, const float effectScale) {
	switch (effectType) {
	case ePhysicsEffectType::GRAVITY_SCALE: {
		auto* targetEntity = Game::entityManager->GetEntity(target);
		if (targetEntity) {
			auto* controllablePhysicsComponent = targetEntity->GetComponent<ControllablePhysicsComponent>();
			// dont want to apply an effect to nothing.
			if (!controllablePhysicsComponent) return;
			controllablePhysicsComponent->SetGravityScale(effectScale);
			GameMessages::SendSetGravityScale(target, effectScale, targetEntity->GetSystemAddress());
		}
		break;
	}

	case ePhysicsEffectType::ATTRACT:
	case ePhysicsEffectType::FRICTION:
	case ePhysicsEffectType::PUSH:
	case ePhysicsEffectType::REPULSE:
	default:
		break;
	}
	// The other types are not handled by the server and are here to handle all cases of the enum.
}

void PhantomPhysicsComponent::Update(float deltaTime) {
	if (!m_dpEntity) return;

	//Process enter events
	for (const auto id : m_dpEntity->GetNewObjects()) {
		ApplyCollisionEffect(id, m_EffectType, m_DirectionalMultiplier);
		m_Parent->OnCollisionPhantom(id);

		//If we are a respawn volume, inform the client:
		if (m_IsRespawnVolume) {
			auto* const entity = Game::entityManager->GetEntity(id);

			if (entity) {
				GameMessages::SendPlayerReachedRespawnCheckpoint(entity, m_RespawnPos, m_RespawnRot);
				entity->SetRespawnPos(m_RespawnPos);
				entity->SetRespawnRot(m_RespawnRot);
			}
		}
	}

	//Process exit events
	for (const auto id : m_dpEntity->GetRemovedObjects()) {
		ApplyCollisionEffect(id, m_EffectType, 1.0f);
		m_Parent->OnCollisionLeavePhantom(id);
	}
}

void PhantomPhysicsComponent::SetDirection(const NiPoint3& pos) {
	m_Direction = pos;
	m_Direction.x *= m_DirectionalMultiplier;
	m_Direction.y *= m_DirectionalMultiplier;
	m_Direction.z *= m_DirectionalMultiplier;

	m_EffectInfoDirty = true;
	m_IsDirectional = true;
}

void PhantomPhysicsComponent::SpawnVertices() const {
	if (!m_dpEntity) {
		LOG("No dpEntity to spawn vertices for %llu:%i", m_Parent->GetObjectID(), m_Parent->GetLOT());
		return;
	}
	PhysicsComponent::SpawnVertices(m_dpEntity);
}

void PhantomPhysicsComponent::SetDirectionalMultiplier(float mul) {
	m_DirectionalMultiplier = mul;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetEffectType(ePhysicsEffectType type) {
	m_EffectType = type;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetMin(uint32_t min) {
	m_Min = min;
	m_MinMax = true;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetMax(uint32_t max) {
	m_Max = max;
	m_MinMax = true;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetPosition(const NiPoint3& pos) {
	PhysicsComponent::SetPosition(pos);
	if (m_dpEntity) m_dpEntity->SetPosition(pos);
}

void PhantomPhysicsComponent::SetRotation(const NiQuaternion& rot) {
	PhysicsComponent::SetRotation(rot);
	if (m_dpEntity) m_dpEntity->SetRotation(rot);
}
