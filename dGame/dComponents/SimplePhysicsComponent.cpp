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

SimplePhysicsComponent::SimplePhysicsComponent(Entity* parent, uint32_t componentID) : PhysicsComponent(parent) {
	auto* const parentEntity = Game::entityManager->GetEntity(m_Parent);
	
	m_Position = parentEntity->GetDefaultPosition();
	m_Rotation = parentEntity->GetDefaultRotation();

	const auto& climbable_type = parentEntity->GetVar<std::u16string>(u"climbable");
	if (climbable_type == u"wall") {
		SetClimbableType(eClimbableType::CLIMBABLE_TYPE_WALL);
	} else if (climbable_type == u"ladder") {
		SetClimbableType(eClimbableType::CLIMBABLE_TYPE_LADDER);
	} else if (climbable_type == u"wallstick") {
		SetClimbableType(eClimbableType::CLIMBABLE_TYPE_WALL_STICK);
	} else {
		SetClimbableType(eClimbableType::CLIMBABLE_TYPE_NOT);
	}
}

SimplePhysicsComponent::~SimplePhysicsComponent() {
}

void SimplePhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	if (bIsInitialUpdate) {
		outBitStream->Write(m_ClimbableType != eClimbableType::CLIMBABLE_TYPE_NOT);
		outBitStream->Write(m_ClimbableType);
	}

	outBitStream->Write(m_DirtyVelocity || bIsInitialUpdate);
	if (m_DirtyVelocity || bIsInitialUpdate) {
		outBitStream->Write(m_Velocity);
		outBitStream->Write(m_AngularVelocity);

		m_DirtyVelocity = false;
	}

	// Physics motion state
	if (m_PhysicsMotionState != 0) {
		outBitStream->Write1();
		outBitStream->Write<uint32_t>(m_PhysicsMotionState);
	} else {
		outBitStream->Write0();
	}
	PhysicsComponent::Serialize(outBitStream, bIsInitialUpdate);
}

uint32_t SimplePhysicsComponent::GetPhysicsMotionState() const {
	return m_PhysicsMotionState;
}

void SimplePhysicsComponent::SetPhysicsMotionState(uint32_t value) {
	m_PhysicsMotionState = value;
}
