#include "PossessorComponent.h"

PossessorComponent::PossessorComponent(Entity* parent) : Component(parent)
{
    m_Possessable = LWOOBJID_EMPTY;
}

PossessorComponent::~PossessorComponent() 
{
    
}

void PossessorComponent::SetPossessable(LWOOBJID value) 
{
    m_Possessable = value;
}

LWOOBJID PossessorComponent::GetPossessable() const
{
    return m_Possessable;
}

void PossessorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) 
{
    outBitStream->Write(m_Possessable != LWOOBJID_EMPTY);
    if (m_Possessable != LWOOBJID_EMPTY)
    {
        outBitStream->Write(m_Possessable);
    }
}

void PossessorComponent::Update(float deltaTime) 
{
    
}
