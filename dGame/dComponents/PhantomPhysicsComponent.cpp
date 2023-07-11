/*
 * Darkflame Universe
 * Copyright 2018
 */

#include <sstream>
#include <iostream>

#include "PhantomPhysicsComponent.h"
#include "Game.h"
#include "LDFFormat.h"
#include "dLogger.h"
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
#include "NiPoint3.h"
#include "NiQuaternion.h"

PhantomPhysicsComponent::PhantomPhysicsComponent(Entity* parent) : Component(parent) {
	m_Position = m_ParentEntity->GetDefaultPosition();
	m_Rotation = m_ParentEntity->GetDefaultRotation();
	m_Scale = m_ParentEntity->GetDefaultScale();
	m_dpEntity = nullptr;

	m_EffectInfoDirty = false;
	m_PositionInfoDirty = false;

	m_IsPhysicsEffectActive = false;
	m_EffectType = ePhysicsEffectType::PUSH;
	m_DirectionalMultiplier = 0.0f;

	m_MinMax = false;
	m_Min = 0;
	m_Max = 1;

	m_IsDirectional = false;
	m_Direction = NiPoint3::ZERO;
}

PhantomPhysicsComponent::~PhantomPhysicsComponent() {
	if (m_dpEntity) dpWorld::Instance().RemoveEntity(m_dpEntity);
}

void PhantomPhysicsComponent::LoadTemplateData() {
	// HF - RespawnPoints. Legacy respawn entity.
	if (m_ParentEntity->GetLOT() == LOT_LEGACY_RESPAWN_POINT) {
		m_IsRespawnVolume = true;
		m_RespawnPos = m_Position;
		m_RespawnRot = m_Rotation;
	}

	auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	auto componentID = compRegistryTable->GetByIDAndType(m_ParentEntity->GetLOT(), eReplicaComponentType::PHANTOM_PHYSICS);

	auto* physCompTable = CDClientManager::Instance().GetTable<CDPhysicsComponentTable>();

	if (!physCompTable) return;

	auto* info = physCompTable->GetByID(componentID);
	if (!info || info->physicsAsset.empty() || info->physicsAsset == "NO_PHYSICS") return;

	if (info->physicsAsset == "miscellaneous\\misc_phys_10x1x5.hkx") {
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 10.0f, 5.0f, 1.0f);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position);
	} else if (info->physicsAsset == "miscellaneous\\misc_phys_640x640.hkx") {
		// Move this down by 13.521004 units so it is still effectively at the same height as before
		m_Position = m_Position - NiPoint3::UNIT_Y * 13.521004f;
		// TODO Fix physics simulation to do simulation at high velocities due to bullet through paper problem...
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 1638.4f, 13.521004f * 2.0f, 1638.4f);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position);
	} else if (info->physicsAsset == "env\\trigger_wall_tall.hkx") {
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 10.0f, 25.0f, 1.0f);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position);
	} else if (info->physicsAsset == "env\\env_gen_placeholderphysics.hkx") {
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 20.0f, 20.0f, 20.0f);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position);
	} else if (info->physicsAsset == "env\\POI_trigger_wall.hkx") {
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 1.0f, 12.5f, 20.0f); // Not sure what the real size is
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position);
	} else if (info->physicsAsset == "env\\NG_NinjaGo\\env_ng_gen_gate_chamber_puzzle_ceiling_tile_falling_phantom.hkx") {
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 18.0f, 5.0f, 15.0f);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position + m_Rotation.GetForwardVector() * 7.5f);
	} else if (info->physicsAsset == "env\\NG_NinjaGo\\ng_flamejet_brick_phantom.HKX") {
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 1.0f, 1.0f, 12.0f);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position + m_Rotation.GetForwardVector() * 6.0f);
	} else if (info->physicsAsset == "env\\Ring_Trigger.hkx") {
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 6.0f, 6.0f, 6.0f);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position);
	} else if (info->physicsAsset == "env\\vfx_propertyImaginationBall.hkx") {
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 4.5f);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position);
	} else if (info->physicsAsset == "env\\env_won_fv_gas-blocking-volume.hkx") {
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 390.496826f, 111.467964f, 600.821534f, true);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_Position.y -= (111.467964f * m_Scale) / 2;
		m_dpEntity->SetPosition(m_Position);
	} else {
		Game::logger->LogDebug("PhantomPhysicsComponent", "This component is supposed to have asset %s but is defaulting to fallback cube.", info->physicsAsset.c_str());
		//add fallback cube:
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), 2.0f, 2.0f, 2.0f);
		m_dpEntity->SetScale(m_Scale);
		m_dpEntity->SetRotation(m_Rotation);
		m_dpEntity->SetPosition(m_Position);
	}
	dpWorld::Instance().AddEntity(m_dpEntity);

}

void PhantomPhysicsComponent::LoadConfigData() {
	if (m_ParentEntity->GetVar<bool>(u"create_physics")) {
		CreatePhysics();
	}

	if (m_ParentEntity->GetVar<bool>(u"respawnVol")) {
		m_IsRespawnVolume = true;
	}

	if (m_IsRespawnVolume) {
		auto respawnPosSplit = GeneralUtils::SplitString(m_ParentEntity->GetVarAsString(u"rspPos"), '\x1f');
		m_RespawnPos = NiPoint3::ZERO;
		if (respawnPosSplit.size() >= 3) {
			GeneralUtils::TryParse(respawnPosSplit[0], m_RespawnPos.x);
			GeneralUtils::TryParse(respawnPosSplit[1], m_RespawnPos.y);
			GeneralUtils::TryParse(respawnPosSplit[2], m_RespawnPos.z);
		}

		auto respawnRotSplit = GeneralUtils::SplitString(m_ParentEntity->GetVarAsString(u"rspRot"), '\x1f');
		m_RespawnRot = NiQuaternion::IDENTITY;
		if (respawnRotSplit.size() >= 4) {
			GeneralUtils::TryParse(respawnRotSplit[0], m_RespawnRot.w);
			GeneralUtils::TryParse(respawnRotSplit[1], m_RespawnRot.x);
			GeneralUtils::TryParse(respawnRotSplit[2], m_RespawnRot.y);
			GeneralUtils::TryParse(respawnRotSplit[3], m_RespawnRot.z);
		}
	}
}

void PhantomPhysicsComponent::CreatePhysics() {
	int32_t type = -1;
	NiPoint3 pos;
	float width = 0.0f; //aka "radius"
	float height = 0.0f;

	if (m_ParentEntity->HasVar(u"primitiveModelType")) {
		type = m_ParentEntity->GetVar<int32_t>(u"primitiveModelType");
		pos.x = m_ParentEntity->GetVar<float>(u"primitiveModelValueX");
		pos.y = m_ParentEntity->GetVar<float>(u"primitiveModelValueY");
		pos.z = m_ParentEntity->GetVar<float>(u"primitiveModelValueZ");
	} else {
		auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
		auto componentID = compRegistryTable->GetByIDAndType(m_ParentEntity->GetLOT(), eReplicaComponentType::PHANTOM_PHYSICS);

		auto* physCompTable = CDClientManager::Instance().GetTable<CDPhysicsComponentTable>();

		if (!physCompTable) return;

		auto info = physCompTable->GetByID(componentID);

		if (!info) return;

		type = info->pcShapeType;
		width = info->playerRadius;
		height = info->playerHeight;
	}

	switch (type) {
	case 1: { //Make a new box shape
		BoxDimensions boxSize(pos.x, pos.y, pos.z);
		if (pos.x == 0.0f) {
			//LU has some weird values, so I think it's best to scale them down a bit
			if (height < 0.5f) height = 2.0f;
			if (width < 0.5f) width = 2.0f;

			//Scale them:
			width = width * m_Scale;
			height = height * m_Scale;

			boxSize = BoxDimensions(width, height, width);
		}
		if (m_dpEntity) delete m_dpEntity;
		m_dpEntity = new dpEntity(m_ParentEntity->GetObjectID(), boxSize);
		break;
	}
	default: {
		Game::logger->Log("PhantomPhysicsComponent", "Unknown shape type: %d", type);
		break;
	}
	}

	if (!m_dpEntity) return;

	m_dpEntity->SetPosition(NiPoint3(m_Position.x, m_Position.y - (height / 2), m_Position.z));

	dpWorld::Instance().AddEntity(m_dpEntity);

	m_HasCreatedPhysics = true;
}

void PhantomPhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_PositionInfoDirty || bIsInitialUpdate);
	if (m_PositionInfoDirty || bIsInitialUpdate) {
		outBitStream->Write(m_Position.x);
		outBitStream->Write(m_Position.y);
		outBitStream->Write(m_Position.z);

		outBitStream->Write(m_Rotation.x);
		outBitStream->Write(m_Rotation.y);
		outBitStream->Write(m_Rotation.z);
		outBitStream->Write(m_Rotation.w);

		if (!bIsInitialUpdate) m_PositionInfoDirty = false;
	}

	outBitStream->Write(m_EffectInfoDirty || bIsInitialUpdate);
	if (m_EffectInfoDirty || bIsInitialUpdate) {
		outBitStream->Write(m_IsPhysicsEffectActive);

		if (m_IsPhysicsEffectActive) {
			outBitStream->Write(m_EffectType);
			outBitStream->Write(m_DirectionalMultiplier);

			// distance info.  Option.
			outBitStream->Write0();
			//outBitStream->Write(m_MinMax);
			//if (m_MinMax) {
				//outBitStream->Write(m_Min);
				//outBitStream->Write(m_Max);
			//}

			outBitStream->Write(m_IsDirectional);
			if (m_IsDirectional) {
				outBitStream->Write(m_Direction.x);
				outBitStream->Write(m_Direction.y);
				outBitStream->Write(m_Direction.z);
			}
		}

		if (!bIsInitialUpdate) m_EffectInfoDirty = false;
	}
}

void PhantomPhysicsComponent::ResetFlags() {
	m_EffectInfoDirty = false;
	m_PositionInfoDirty = false;
}

void PhantomPhysicsComponent::Update(float deltaTime) {
	if (!m_dpEntity) return;

	//Process enter events
	for (auto* en : m_dpEntity->GetNewObjects()) {
		m_ParentEntity->OnCollisionPhantom(en->GetObjectID());

		//If we are a respawn volume, inform the client:
		if (!m_IsRespawnVolume) continue;
		auto entity = EntityManager::Instance()->GetEntity(en->GetObjectID());

		if (!entity) continue;

		GameMessages::SendPlayerReachedRespawnCheckpoint(entity, m_RespawnPos, m_RespawnRot);
		entity->SetRespawnPosition(m_RespawnPos);
		entity->SetRespawnRotation(m_RespawnRot);
	}

	//Process exit events
	for (auto* en : m_dpEntity->GetRemovedObjects()) {
		m_ParentEntity->OnCollisionLeavePhantom(en->GetObjectID());
	}
}

void PhantomPhysicsComponent::SetDirection(const NiPoint3& pos) {
	if (m_Direction == pos) return;
	m_Direction = pos;
	m_Direction.x *= m_DirectionalMultiplier;
	m_Direction.y *= m_DirectionalMultiplier;
	m_Direction.z *= m_DirectionalMultiplier;

	m_EffectInfoDirty = true;
	m_IsDirectional = true;
}

void PhantomPhysicsComponent::SpawnVertices() {
	if (!m_dpEntity) return;

	Game::logger->Log("PhantomPhysicsComponent", "objectId is %llu", m_ParentEntity->GetObjectID());
	auto box = dynamic_cast<dpShapeBox*>(m_dpEntity->GetShape());
	if (!box) return;
	for (auto vert : box->GetVertices()) {
		Game::logger->Log("PhantomPhysicsComponent", "%f, %f, %f", vert.x, vert.y, vert.z);

		EntityInfo info;
		info.lot = 33;
		info.pos = vert;
		info.spawner = nullptr;
		info.spawnerID = m_ParentEntity->GetObjectID();
		info.spawnerNodeID = 0;

		Entity* newEntity = EntityManager::Instance()->CreateEntity(info);
		EntityManager::Instance()->ConstructEntity(newEntity);
	}
}

void PhantomPhysicsComponent::SetDirectionalMultiplier(float mul) {
	if (mul == m_DirectionalMultiplier) return;
	m_DirectionalMultiplier = mul;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetEffectType(const ePhysicsEffectType type) {
	if (type == m_EffectType) return;
	m_EffectType = type;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetMin(const uint32_t min) {
	if (min == m_Min) return;
	m_Min = min;
	m_MinMax = true;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetMax(const uint32_t max) {
	if (max == m_Max) return;
	m_Max = max;
	m_MinMax = true;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetPosition(const NiPoint3& pos) {
	if (pos == m_Position) return;
	m_Position = pos;
	m_PositionInfoDirty = true;
	if (m_dpEntity) m_dpEntity->SetPosition(pos);
}

void PhantomPhysicsComponent::SetRotation(const NiQuaternion& rot) {
	if (rot == m_Rotation) return;
	m_Rotation = rot;
	m_PositionInfoDirty = true;
	if (m_dpEntity) m_dpEntity->SetRotation(rot);
}
