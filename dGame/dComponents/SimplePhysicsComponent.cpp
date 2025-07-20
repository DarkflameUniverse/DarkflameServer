/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "SimplePhysicsComponent.h"
#include "BitStream.h"
#include "Game.h"
#include "Logger.h"
#include "dpWorld.h"
#include "CDClientManager.h"
#include "CDPhysicsComponentTable.h"

#include "Entity.h"
#include "StringifiedEnum.h"
#include "Amf3.h"

SimplePhysicsComponent::SimplePhysicsComponent(Entity* parent, int32_t componentID) : PhysicsComponent(parent, componentID) {
	RegisterMsg(MessageType::Game::GET_OBJECT_REPORT_INFO, this, &SimplePhysicsComponent::OnGetObjectReportInfo);

	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();

	const auto& climbable_type = m_Parent->GetVar<std::u16string>(u"climbable");
	if (climbable_type == u"wall") {
		SetClimbableType(eClimbableType::CLIMBABLE_TYPE_WALL);
	} else if (climbable_type == u"ladder") {
		SetClimbableType(eClimbableType::CLIMBABLE_TYPE_LADDER);
	} else if (climbable_type == u"wallstick") {
		SetClimbableType(eClimbableType::CLIMBABLE_TYPE_WALL_STICK);
	} else {
		SetClimbableType(eClimbableType::CLIMBABLE_TYPE_NOT);
	}
	m_PhysicsMotionState = m_Parent->GetVarAs<uint32_t>(u"motionType");
}

SimplePhysicsComponent::~SimplePhysicsComponent() {
}

void SimplePhysicsComponent::Update(const float deltaTime) {
	if (m_Velocity == NiPoint3Constant::ZERO) return;
	m_Position += m_Velocity * deltaTime;
	m_DirtyPosition = true;
	Game::entityManager->SerializeEntity(m_Parent);
}

void SimplePhysicsComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	if (bIsInitialUpdate) {
		outBitStream.Write(m_ClimbableType != eClimbableType::CLIMBABLE_TYPE_NOT);
		outBitStream.Write(m_ClimbableType);
	}

	outBitStream.Write(m_DirtyVelocity || bIsInitialUpdate);
	if (m_DirtyVelocity || bIsInitialUpdate) {
		outBitStream.Write(m_Velocity);
		outBitStream.Write(m_AngularVelocity);

		m_DirtyVelocity = false;
	}

	// Physics motion state
	outBitStream.Write(m_DirtyPhysicsMotionState || bIsInitialUpdate);
	if (m_DirtyPhysicsMotionState || bIsInitialUpdate) {
		outBitStream.Write<uint32_t>(m_PhysicsMotionState);
		m_DirtyPhysicsMotionState = false;
	}
	PhysicsComponent::Serialize(outBitStream, bIsInitialUpdate);
}

uint32_t SimplePhysicsComponent::GetPhysicsMotionState() const {
	return m_PhysicsMotionState;
}

void SimplePhysicsComponent::SetPhysicsMotionState(uint32_t value) {
	m_DirtyPhysicsMotionState = m_PhysicsMotionState != value;
	m_PhysicsMotionState = value;
}

bool SimplePhysicsComponent::OnGetObjectReportInfo(GameMessages::GameMsg& msg) {
	PhysicsComponent::OnGetObjectReportInfo(msg);
	auto& reportInfo = static_cast<GameMessages::GetObjectReportInfo&>(msg);
	auto& info = reportInfo.subCategory->PushDebug("Simple Physics Info");
	auto& velocity = info.PushDebug("Velocity");
	velocity.PushDebug<AMFDoubleValue>("x") = m_Velocity.x;
	velocity.PushDebug<AMFDoubleValue>("y") = m_Velocity.y;
	velocity.PushDebug<AMFDoubleValue>("z") = m_Velocity.z;
	auto& angularVelocity = info.PushDebug("Angular Velocity");
	angularVelocity.PushDebug<AMFDoubleValue>("x") = m_AngularVelocity.x;
	angularVelocity.PushDebug<AMFDoubleValue>("y") = m_AngularVelocity.y;
	angularVelocity.PushDebug<AMFDoubleValue>("z") = m_AngularVelocity.z;
	info.PushDebug<AMFIntValue>("Physics Motion State") = m_PhysicsMotionState;
	info.PushDebug<AMFStringValue>("Climbable Type") = StringifiedEnum::ToString(m_ClimbableType).data();
	return true;
}
