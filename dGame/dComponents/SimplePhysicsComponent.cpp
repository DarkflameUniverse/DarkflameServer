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

SimplePhysicsComponent::SimplePhysicsComponent(Entity* parent, int32_t componentID) : PhysicsComponent(parent, componentID) {
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
