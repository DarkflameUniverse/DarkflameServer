/*
 * Darkflame Universe
 * Copyright 2018
 */

#include <algorithm>
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
#include "MovementMessages.h"
#include "ePhysicsEffectType.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"
#include "dServer.h"
#include "EntityInfo.h"
#include "Amf3.h"

#include "dpWorld.h"
#include "dpEntity.h"
#include "dpShapeBox.h"
#include "dpShapeSphere.h"
#include "ObjectLoadState.h"

PhantomPhysicsComponent::PhantomPhysicsComponent(Entity* parent, const int32_t componentID) : PhysicsComponent(parent, componentID) {
	RegisterMsg(&PhantomPhysicsComponent::OnGetObjectReportInfo);

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
}

// After the physics config: the volume's blocker, respawn settings and its physics
void PhantomPhysicsComponent::LoadConfigData() {
	PhysicsComponent::LoadConfigData();

	// A phantom volume enemies can't walk into when it carves the navmesh (e.g. the Sentinel camp walls)
	RegisterMovementBlocker(ComponentType, false, m_Scale);

	if (m_Parent->GetVar<bool>(u"create_physics")) {
		m_dpEntity = CreatePhysicsLnv(m_Scale, ComponentType);
	}

	if (m_Parent->GetVar<bool>(u"respawnVol")) {
		m_IsRespawnVolume = true;
	}

	if (m_IsRespawnVolume) {
		const auto respawnPos = GeneralUtils::SplitString(m_Parent->GetVarAsString(u"rspPos"), '\x1f');
		m_RespawnPos = GeneralUtils::TryParse(respawnPos, NiPoint3Constant::ZERO);

		const auto respawnRot = GeneralUtils::SplitString(m_Parent->GetVarAsString(u"rspRot"), '\x1f');
		m_RespawnRot = respawnRot.size() >= 4 ? NiQuaternion(
			GeneralUtils::TryParse(respawnRot[0], 1.0f),
			GeneralUtils::TryParse(respawnRot[1], 0.0f),
			GeneralUtils::TryParse(respawnRot[2], 0.0f),
			GeneralUtils::TryParse(respawnRot[3], 0.0f))
			: QuatUtils::IDENTITY;
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

void ApplyCollisionEffect(const LWOOBJID& target, const ePhysicsEffectType effectType, const float effectScale);

PhantomPhysicsComponent::~PhantomPhysicsComponent() {
	if (!m_dpEntity) return;

	if (m_PhysicsActive) dpWorld::RemoveEntity(m_dpEntity);
	else delete m_dpEntity;
}

void PhantomPhysicsComponent::SetPhysicsActive(const bool active) {
	if (!m_dpEntity || m_PhysicsActive == active) return;
	m_PhysicsActive = active;

	if (active) {
		// Anything already inside enters on the next physics step
		dpWorld::AddEntity(m_dpEntity);
		return;
	}

	dpWorld::DetachEntity(m_dpEntity);
	const std::vector<LWOOBJID> inside(m_dpEntity->GetCurrentlyCollidingObjects().begin(), m_dpEntity->GetCurrentlyCollidingObjects().end());
	m_dpEntity->ClearCollisions();
	for (const auto id : inside) {
		ApplyCollisionEffect(id, m_EffectType, 1.0f);
		m_Parent->OnCollisionLeavePhantom(id);
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

			// Live wrote the distances as the client keeps them: "true,150,175" went out as 1 and 175
			outBitStream.Write(m_MinMax);
			if (m_MinMax) {
				const float min = std::min(m_Min, 1.0f);
				outBitStream.Write(min);
				outBitStream.Write(std::max(m_Max, min));
			}

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
			GameMessages::SetGravityScale gravityScale;
			gravityScale.target = target;
			gravityScale.scale = effectScale;
			gravityScale.SendToClient(targetEntity->GetSystemAddress());
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

void PhantomPhysicsComponent::Run(float deltaTime) {
	if (!m_dpEntity || !m_PhysicsActive) return;

	//Process enter events
	for (const auto id : m_dpEntity->GetNewObjects()) {
		ApplyCollisionEffect(id, m_EffectType, m_DirectionalMultiplier);
		m_Parent->OnCollisionPhantom(id);

		//If we are a respawn volume, inform the client:
		if (m_IsRespawnVolume) {
			auto* const entity = Game::entityManager->GetEntity(id);

			if (entity) {
				GameMessages::PlayerReachedRespawnCheckpoint respawnCheckpoint;
				respawnCheckpoint.target = entity->GetObjectID();
				respawnCheckpoint.pos = m_RespawnPos;
				respawnCheckpoint.rot = m_RespawnRot;
				respawnCheckpoint.SendToClient(entity->GetSystemAddress());
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

void PhantomPhysicsComponent::SetMin(const float min) {
	m_Min = min;
	m_MinMax = true;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetMax(const float max) {
	m_Max = max;
	m_MinMax = true;
	m_EffectInfoDirty = true;
}

void PhantomPhysicsComponent::SetVolumeEffect(std::vector<std::string> args) {
	for (auto& arg : args) {
		arg.erase(0, arg.find_first_not_of(' '));
		arg.erase(arg.find_last_not_of(' ') + 1);
	}

	SetPhysicsEffectActive(true);
	ePhysicsEffectType effectType = ePhysicsEffectType::PUSH;
	if (!args.empty()) {
		std::transform(args.at(0).begin(), args.at(0).end(), args.at(0).begin(), ::tolower);
		if (args.at(0) == "push") effectType = ePhysicsEffectType::PUSH;
		else if (args.at(0) == "attract") effectType = ePhysicsEffectType::ATTRACT;
		else if (args.at(0) == "repulse") effectType = ePhysicsEffectType::REPULSE;
		else if (args.at(0) == "gravity") effectType = ePhysicsEffectType::GRAVITY_SCALE;
		else if (args.at(0) == "friction") effectType = ePhysicsEffectType::FRICTION;
	}

	SetEffectType(effectType);
	if (args.size() > 1) SetDirectionalMultiplier(GeneralUtils::TryParse(args.at(1), 0.0f));
	if (args.size() > 4) {
		// Live sent the direction with the amount as its length ("Push,50,0.91,-0.34,-0.22" as 45.68, -17.07, -11.04)
		// and none when it was zero ("Repulse,150,0,0,0,true,25,100")
		const NiPoint3 direction = GeneralUtils::TryParse(args.at(2), args.at(3), args.at(4), NiPoint3Constant::ZERO);
		if (direction != NiPoint3Constant::ZERO) SetDirection(direction.Unitize());
	}
	// Live sent the distances for "true" only: volumes with "1,20,50" went out without them
	if (args.size() > 7 && args.at(5) == "true") {
		SetMin(GeneralUtils::TryParse(args.at(6), 0.0f));
		SetMax(GeneralUtils::TryParse(args.at(7), 0.0f));
	}
}

void PhantomPhysicsComponent::SetPosition(const NiPoint3& pos) {
	PhysicsComponent::SetPosition(pos);
	if (m_dpEntity) m_dpEntity->SetPosition(pos);
}

void PhantomPhysicsComponent::SetRotation(const NiQuaternion& rot) {
	PhysicsComponent::SetRotation(rot);
	if (m_dpEntity) m_dpEntity->SetRotation(rot);
}

bool PhantomPhysicsComponent::OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo) {
	PhysicsComponent::OnGetObjectReportInfo(reportInfo);
	if (!reportInfo.subCategory) {
		return false;
	}
	auto& info = reportInfo.subCategory->PushDebug("Phantom Physics Info");
	info.PushDebug<AMFDoubleValue>("Scale") = m_Scale;
	info.PushDebug<AMFBoolValue>("Is Physics Effect Active") = m_IsPhysicsEffectActive;
	info.PushDebug<AMFIntValue>("Effect Type") = static_cast<int>(m_EffectType);
	info.PushDebug<AMFDoubleValue>("Directional Multiplier") = m_DirectionalMultiplier;
	info.PushDebug<AMFBoolValue>("Is Directional") = m_IsDirectional;
	auto& direction = info.PushDebug("Direction").PushDebug(m_Direction);

	if (m_MinMax) {
		auto& minMaxInfo = info.PushDebug("Min Max Info");
		minMaxInfo.PushDebug<AMFDoubleValue>("Min") = m_Min;
		minMaxInfo.PushDebug<AMFDoubleValue>("Max") = m_Max;
	}

	if (m_IsRespawnVolume) {
		auto& respawnInfo = info.PushDebug("Respawn Info");
		respawnInfo.PushDebug<AMFBoolValue>("Is Respawn Volume") = m_IsRespawnVolume;
		respawnInfo.PushDebug("Respawn Position").PushDebug(m_RespawnPos);
		respawnInfo.PushDebug("Respawn Rotation").PushDebug(m_RespawnRot);
	}

	return true;
}

void PhantomPhysicsComponent::Allocator(Entity& entity, ObjectLoadState& state) {
	const auto phantomPhysicsComponentID = state.GetComponentID(eReplicaComponentType::PHANTOM_PHYSICS);
	if (state.hasModelBehaviors) {
		// LWOPhantomPhysicsComponent::Allocator (0x010383b0): left to the model case
		if (phantomPhysicsComponentID > 0) state.modelPhysicsComponentID = phantomPhysicsComponentID;
	} else if (phantomPhysicsComponentID > 0 && !entity.HasComponent(eReplicaComponentType::PHANTOM_PHYSICS)) {
		entity.AddComponent<PhantomPhysicsComponent>(phantomPhysicsComponentID)->SetPhysicsEffectActive(false);
	}
}
