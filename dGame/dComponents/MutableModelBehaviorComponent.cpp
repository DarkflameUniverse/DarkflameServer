#include "MutableModelBehaviorComponent.h"
#include "Entity.h"

MutableModelBehaviorComponent::MutableModelBehaviorComponent(Entity* parent) : Component(parent) {
	m_DirtyModelBehaviorInfo = false;
	m_BehaviorCount = 0;
	m_IsPaused = true;

	m_DirtyModelEditingInfo = false;
	m_OldObjId = LWOOBJID_EMPTY;
	m_Editor = LWOOBJID_EMPTY;

}

void MutableModelBehaviorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_DirtyModelBehaviorInfo || bIsInitialUpdate);
	if (m_DirtyModelBehaviorInfo){
		outBitStream->Write(m_BehaviorCount);
		outBitStream->Write(m_IsPaused);
		m_DirtyModelBehaviorInfo = false;
	}

	outBitStream->Write(m_DirtyModelEditingInfo && bIsInitialUpdate);
	if (m_DirtyModelEditingInfo && bIsInitialUpdate) {
		outBitStream->Write(m_OldObjId);
		outBitStream->Write(m_Editor);
		m_DirtyModelEditingInfo = false;
	}
}

