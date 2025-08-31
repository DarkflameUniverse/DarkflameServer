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
	using namespace GameMessages;
	RegisterMsg<GetObjectReportInfo>(this, &SimplePhysicsComponent::OnGetObjectReportInfo);
	RegisterMsg<GameMessages::GetAngularVelocity>(this, &SimplePhysicsComponent::OnGetAngularVelocity);
	RegisterMsg<GameMessages::SetAngularVelocity>(this, &SimplePhysicsComponent::OnSetAngularVelocity);

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
	if (m_Velocity != NiPoint3Constant::ZERO) {
		m_Position += m_Velocity * deltaTime;
		m_DirtyPosition = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}

	if (m_AngularVelocity != NiPoint3Constant::ZERO) {
		m_Rotation.Normalize();
		const auto vel = NiQuaternion::FromEulerAngles(m_AngularVelocity * deltaTime);
		m_Rotation *= vel;
		const auto euler = m_Rotation.GetEulerAngles();
		m_DirtyPosition = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}
}

void SimplePhysicsComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	if (bIsInitialUpdate) {
		outBitStream.Write(m_ClimbableType != eClimbableType::CLIMBABLE_TYPE_NOT);
		outBitStream.Write(m_ClimbableType);
	}

	outBitStream.Write(m_DirtyVelocity || bIsInitialUpdate);
	if (m_DirtyVelocity || bIsInitialUpdate) {
		outBitStream.Write(m_Velocity.x);
		outBitStream.Write(m_Velocity.y);
		outBitStream.Write(m_Velocity.z);
		outBitStream.Write(m_AngularVelocity.x);
		outBitStream.Write(m_AngularVelocity.y);
		outBitStream.Write(m_AngularVelocity.z);

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

bool SimplePhysicsComponent::OnSetAngularVelocity(GameMessages::GameMsg& msg) {
	auto& setAngVel = static_cast<GameMessages::SetAngularVelocity&>(msg);
	m_DirtyVelocity |= setAngVel.bForceFlagDirty || (m_AngularVelocity != setAngVel.angVelocity);
	m_AngularVelocity = setAngVel.angVelocity;
	LOG("Velocity is now %f %f %f", m_AngularVelocity.x, m_AngularVelocity.y, m_AngularVelocity.z);
	Game::entityManager->SerializeEntity(m_Parent);
	return true;
}

bool SimplePhysicsComponent::OnGetAngularVelocity(GameMessages::GameMsg& msg) {
	auto& getAngVel = static_cast<GameMessages::GetAngularVelocity&>(msg);
	getAngVel.angVelocity = m_AngularVelocity;
	return true;
}
