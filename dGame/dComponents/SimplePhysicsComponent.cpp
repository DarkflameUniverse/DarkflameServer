/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "SimplePhysicsComponent.h"
#include "BitStream.h"
#include "Game.h"
#include "dLogger.h"
#include "dpWorld.h"
#include "CDClientManager.h"
#include "CDPhysicsComponentTable.h"

#include "Entity.h"

SimplePhysicsComponent::SimplePhysicsComponent(uint32_t componentID, Entity* parent) : Component(parent) {
    m_Position = m_Parent->GetDefaultPosition();
    m_Rotation = m_Parent->GetDefaultRotation();
    m_IsDirty = true;
}

SimplePhysicsComponent::~SimplePhysicsComponent() {
}

void SimplePhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
    if (bIsInitialUpdate) {
        outBitStream->Write0(); // climbable
        outBitStream->Write<int32_t>(0); // climbableType
    }
    
    outBitStream->Write(m_DirtyVelocity || bIsInitialUpdate);
    if (m_DirtyVelocity || bIsInitialUpdate) {
        outBitStream->Write(m_Velocity);
        outBitStream->Write(m_AngularVelocity);

        m_DirtyVelocity = false;
    }

    // Physics motion state
    if (m_PhysicsMotionState != 0)
    {
        outBitStream->Write1();
        outBitStream->Write<uint32_t>(m_PhysicsMotionState);
    }
    else
    {
        outBitStream->Write0();
    }
    
    outBitStream->Write(m_IsDirty || bIsInitialUpdate);
    if (m_IsDirty || bIsInitialUpdate) {
        outBitStream->Write(m_Position.x);
        outBitStream->Write(m_Position.y);
        outBitStream->Write(m_Position.z);
        outBitStream->Write(m_Rotation.x);
        outBitStream->Write(m_Rotation.y);
        outBitStream->Write(m_Rotation.z);
        outBitStream->Write(m_Rotation.w);

        m_IsDirty = false;
    }
}

void SimplePhysicsComponent::Update(float deltaTime) {
    if (GetAngularVelocity() != NiPoint3::ZERO) {
        // TODO
    }

    if (GetVelocity() != NiPoint3::ZERO) {
        Game::logger->Log("SPC", "Position if x %f y %f z %f\n", m_Position.x, m_Position.y, m_Position.z);
        m_Position = m_Position + (GetVelocity() * deltaTime);
    }
}

uint32_t SimplePhysicsComponent::GetPhysicsMotionState() const
{
    return m_PhysicsMotionState;
}

void SimplePhysicsComponent::SetPhysicsMotionState(uint32_t value) 
{
    m_PhysicsMotionState = value;
}
