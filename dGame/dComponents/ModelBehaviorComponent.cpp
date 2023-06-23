#include "ModelBehaviorComponent.h"
#include "Entity.h"
#include "ePhysicsBehaviorType.h"

ModelBehaviorComponent::ModelBehaviorComponent(Entity* parent) : Component(parent) {
	m_DirtyModelInfo = false;
	m_IsPickable = false;
	m_PhysicsType = ePhysicsBehaviorType::STANDARD;
	m_OriginalPosition = m_ParentEntity->GetDefaultPosition();
	m_OriginalRotation = m_ParentEntity->GetDefaultRotation();
}

void ModelBehaviorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_DirtyModelInfo || bIsInitialUpdate);
	if (m_DirtyModelInfo || bIsInitialUpdate) {
		outBitStream->Write(m_IsPickable);
		outBitStream->Write(m_PhysicsType);
		outBitStream->Write(m_OriginalPosition);
		outBitStream->Write(m_OriginalRotation);
		m_DirtyModelInfo = false;
	}
}

