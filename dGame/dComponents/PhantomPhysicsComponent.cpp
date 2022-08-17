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

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"
#include "dServer.h"

#include "dpWorld.h"
#include "dpEntity.h"
#include "dpShapeBox.h"
#include "dpShapeSphere.h"

PhantomPhysicsComponent::PhantomPhysicsComponent(Entity* parent) : Component(parent) {
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();
	m_Scale = m_Parent->GetDefaultScale();
	m_dpEntity = nullptr;

	m_EffectInfoDirty = false;
	m_PositionInfoDirty = false;

	m_IsPhysicsEffectActive = false;
	m_EffectType = 0;
	m_DirectionalMultiplier = 0.0f;

	m_MinMax = false;
	m_Min = 0;
	m_Max = 1;

	m_IsDirectional = false;
	m_Direction = NiPoint3(); // * m_DirectionalMultiplier

	if (m_Parent->GetVar<bool>(u"create_physics")) {
		CreatePhysics();
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

	/*
	for (LDFBaseData* data : settings) {
		if (data) {
			if (data->GetKey() == u"create_physics") {
				if (bool(std::stoi(data->GetValueAsString()))) {
					CreatePhysics(settings);
				}
			}

			if (data->GetKey() == u"respawnVol") {
				if (bool(std::stoi(data->GetValueAsString()))) {
					m_IsRespawnVolume = true;
				}
			}

			if (m_IsRespawnVolume) {
				if (data->GetKey() == u"rspPos") {
					//Joy, we get to split strings!
					std::stringstream test(data->GetValueAsString());
					std::string segment;
					std::vector<std::string> seglist;

					while (std::getline(test, segment, '\x1f')) {
						seglist.push_back(segment);
					}

					m_RespawnPos = NiPoint3(std::stof(seglist[0]), std::stof(seglist[1]), std::stof(seglist[2]));
				}

				if (data->GetKey() == u"rspRot") {
					//Joy, we get to split strings!
					std::stringstream test(data->GetValueAsString());
					std::string segment;
					std::vector<std::string> seglist;

					while (std::getline(test, segment, '\x1f')) {
						seglist.push_back(segment);
					}

					m_RespawnRot = NiQuaternion(std::stof(seglist[0]), std::stof(seglist[1]), std::stof(seglist[2]), std::stof(seglist[3]));
				}
			}

			if (m_Parent->GetLOT() == 4945) // HF - RespawnPoints
			{
				m_IsRespawnVolume = true;
				m_RespawnPos = m_Position;
				m_RespawnRot = m_Rotation;
			}
		}
	}
	*/

	if (!m_HasCreatedPhysics) {
		CDComponentsRegistryTable* compRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
		auto componentID = compRegistryTable->GetByIDAndType(m_Parent->GetLOT(), COMPONENT_TYPE_PHANTOM_PHYSICS);

		CDPhysicsComponentTable* physComp = CDClientManager::Instance()->GetTable<CDPhysicsComponentTable>("PhysicsComponent");

		if (physComp == nullptr) return;

		auto* info = physComp->GetByID(componentID);
		if (info == nullptr || info->physicsAsset == "" || info->physicsAsset == "NO_PHYSICS") return;

		//temp test
		if (info->physicsAsset == "miscellaneous\\misc_phys_10x1x5.hkx") {
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 10.0f, 5.0f, 1.0f);

			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position);

			dpWorld::Instance().AddEntity(m_dpEntity);
		} else if (info->physicsAsset == "miscellaneous\\misc_phys_640x640.hkx") {
			// Move this down by 13.521004 units so it is still effectively at the same height as before
			m_Position = m_Position - NiPoint3::UNIT_Y * 13.521004f;
			// TODO Fix physics simulation to do simulation at high velocities due to bullet through paper problem...
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 1638.4f, 13.521004f * 2.0f, 1638.4f);

			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position);

			dpWorld::Instance().AddEntity(m_dpEntity);
		} else if (info->physicsAsset == "env\\trigger_wall_tall.hkx") {
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 10.0f, 25.0f, 1.0f);
			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position);
			dpWorld::Instance().AddEntity(m_dpEntity);
		} else if (info->physicsAsset == "env\\env_gen_placeholderphysics.hkx") {
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 20.0f, 20.0f, 20.0f);
			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position);
			dpWorld::Instance().AddEntity(m_dpEntity);
		} else if (info->physicsAsset == "env\\POI_trigger_wall.hkx") {
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 1.0f, 12.5f, 20.0f); // Not sure what the real size is
			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position);
			dpWorld::Instance().AddEntity(m_dpEntity);
		} else if (info->physicsAsset == "env\\NG_NinjaGo\\env_ng_gen_gate_chamber_puzzle_ceiling_tile_falling_phantom.hkx") {
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 18.0f, 5.0f, 15.0f);
			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position + m_Rotation.GetForwardVector() * 7.5f);
			dpWorld::Instance().AddEntity(m_dpEntity);
		} else if (info->physicsAsset == "env\\NG_NinjaGo\\ng_flamejet_brick_phantom.HKX") {
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 1.0f, 1.0f, 12.0f);
			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position + m_Rotation.GetForwardVector() * 6.0f);
			dpWorld::Instance().AddEntity(m_dpEntity);
		} else if (info->physicsAsset == "env\\Ring_Trigger.hkx") {
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 6.0f, 6.0f, 6.0f);
			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position);
			dpWorld::Instance().AddEntity(m_dpEntity);
		} else if (info->physicsAsset == "env\\vfx_propertyImaginationBall.hkx") {
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 4.5f);
			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position);
			dpWorld::Instance().AddEntity(m_dpEntity);
		} else {
			//Game::logger->Log("PhantomPhysicsComponent", "This one is supposed to have %s", info->physicsAsset.c_str());

			//add fallback cube:
			m_dpEntity = new dpEntity(m_Parent->GetObjectID(), 2.0f, 2.0f, 2.0f);
			m_dpEntity->SetScale(m_Scale);
			m_dpEntity->SetRotation(m_Rotation);
			m_dpEntity->SetPosition(m_Position);
			dpWorld::Instance().AddEntity(m_dpEntity);
		}

	}
}

PhantomPhysicsComponent::~PhantomPhysicsComponent() {
	if (m_dpEntity) {
		dpWorld::Instance().RemoveEntity(m_dpEntity);
	}
}

void PhantomPhysicsComponent::CreatePhysics() {
	unsigned char alpha;
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	int type = -1;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float width = 0.0f; //aka "radius"
	float height = 0.0f;

	if (m_Parent->HasVar(u"primitiveModelType")) {
		type = m_Parent->GetVar<int32_t>(u"primitiveModelType");
		x = m_Parent->GetVar<float>(u"primitiveModelValueX");
		y = m_Parent->GetVar<float>(u"primitiveModelValueY");
		z = m_Parent->GetVar<float>(u"primitiveModelValueZ");
	} else {
		CDComponentsRegistryTable* compRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
		auto componentID = compRegistryTable->GetByIDAndType(m_Parent->GetLOT(), COMPONENT_TYPE_PHANTOM_PHYSICS);

		CDPhysicsComponentTable* physComp = CDClientManager::Instance()->GetTable<CDPhysicsComponentTable>("PhysicsComponent");

		if (physComp == nullptr) return;

		auto info = physComp->GetByID(componentID);

		if (info == nullptr) return;

		type = info->pcShapeType;
		width = info->playerRadius;
		height = info->playerHeight;
	}

	switch (type) {
	case 1: { //Make a new box shape
		NiPoint3 boxSize(x, y, z);
		if (x == 0.0f) {
			//LU has some weird values, so I think it's best to scale them down a bit
			if (height < 0.5f) height = 2.0f;
			if (width < 0.5f) width = 2.0f;

			//Scale them:
			width = width * m_Scale;
			height = height * m_Scale;

			boxSize = NiPoint3(width, height, width);
		}

		m_dpEntity = new dpEntity(m_Parent->GetObjectID(), boxSize);
		break;
	}
	}

	if (!m_dpEntity) return;

	m_dpEntity->SetPosition({ m_Position.x, m_Position.y - (height / 2), m_Position.z });

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

		m_PositionInfoDirty = false;
	}

	outBitStream->Write(m_EffectInfoDirty || bIsInitialUpdate);
	if (m_EffectInfoDirty || bIsInitialUpdate) {
		outBitStream->Write(m_IsPhysicsEffectActive);

		if (m_IsPhysicsEffectActive) {
			outBitStream->Write(m_EffectType);
			outBitStream->Write(m_DirectionalMultiplier);

			// forgive me father for i have sinned
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

		m_EffectInfoDirty = false;
	}
}

void PhantomPhysicsComponent::ResetFlags() {
	m_EffectInfoDirty = false;
	m_PositionInfoDirty = false;
}

void PhantomPhysicsComponent::Update(float deltaTime) {
	if (!m_dpEntity) return;

	//Process enter events
	for (auto en : m_dpEntity->GetNewObjects()) {
		m_Parent->OnCollisionPhantom(en->GetObjectID());

		//If we are a respawn volume, inform the client:
		if (m_IsRespawnVolume) {
			auto entity = EntityManager::Instance()->GetEntity(en->GetObjectID());

			if (entity) {
				GameMessages::SendPlayerReachedRespawnCheckpoint(entity, m_RespawnPos, m_RespawnRot);
				entity->SetRespawnPos(m_RespawnPos);
				entity->SetRespawnRot(m_RespawnRot);
			}
		}
	}

	//Process exit events
	for (auto en : m_dpEntity->GetRemovedObjects()) {
		m_Parent->OnCollisionLeavePhantom(en->GetObjectID());
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

void PhantomPhysicsComponent::SpawnVertices() {
	if (!m_dpEntity) return;

	std::cout << m_Parent->GetObjectID() << std::endl;
	auto box = static_cast<dpShapeBox*>(m_dpEntity->GetShape());
	for (auto vert : box->GetVertices()) {
		std::cout << vert.x << ", " << vert.y << ", " << vert.z << std::endl;

		EntityInfo info;
		info.lot = 33;
		info.pos = vert;
		info.spawner = nullptr;
		info.spawnerID = m_Parent->GetObjectID();
		info.spawnerNodeID = 0;

		Entity* newEntity = EntityManager::Instance()->CreateEntity(info, nullptr);
		EntityManager::Instance()->ConstructEntity(newEntity);
	}
}

void PhantomPhysicsComponent::SetDirectionalMultiplier(float mul) {
	m_DirectionalMultiplier = mul;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetEffectType(uint32_t type) {
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
	m_Position = pos;

	if (m_dpEntity) m_dpEntity->SetPosition(pos);
}

void PhantomPhysicsComponent::SetRotation(const NiQuaternion& rot) {
	m_Rotation = rot;

	if (m_dpEntity) m_dpEntity->SetRotation(rot);
}
