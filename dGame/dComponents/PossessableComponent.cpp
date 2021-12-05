#include "PossessableComponent.h"

#include "PossessorComponent.h"
#include "EntityManager.h"

PossessableComponent::PossessableComponent(Entity* parent) : Component(parent)
{
    m_Possessor = LWOOBJID_EMPTY;
}

PossessableComponent::~PossessableComponent() 
{
    
}

void PossessableComponent::SetPossessor(LWOOBJID value) 
{
    m_Possessor = value;
}

LWOOBJID PossessableComponent::GetPossessor() const
{
    return m_Possessor;
}

void PossessableComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) 
{
    outBitStream->Write(m_Possessor != LWOOBJID_EMPTY);
    if (m_Possessor != LWOOBJID_EMPTY)
    {
        outBitStream->Write1();
        outBitStream->Write(m_Possessor);
        outBitStream->Write0();
        outBitStream->Write0();
    }
}

void PossessableComponent::Update(float deltaTime) 
{
    
}

void PossessableComponent::OnUse(Entity* originator) {
    PossessorComponent* possessorComponent;
    if (originator->TryGetComponent(COMPONENT_TYPE_POSSESSOR, possessorComponent)) {
        SetPossessor(originator->GetObjectID());
        possessorComponent->SetPossessable(m_Parent->GetObjectID());
        EntityManager::Instance()->SerializeEntity(m_Parent);
        EntityManager::Instance()->SerializeEntity(originator);
    }
}
