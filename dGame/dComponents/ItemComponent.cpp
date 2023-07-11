#include "ItemComponent.h"
#include "Entity.h"
#include "eUgcModerationStatus.h"

ItemComponent::ItemComponent(Entity* parent) : Component(parent) {
	m_ParentEntity = parent;

	m_DirtyItemInfo = false;

	m_UgId = m_ParentEntity->GetVarAs<LWOOBJID>(u"userModelID");
	if (m_UgId == LWOOBJID_EMPTY) m_UgId = m_ParentEntity->GetObjectID();

	m_UgModerationStatus = eUgcModerationStatus::NoStatus;

	m_UgDescription = u"";
}

void ItemComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {

	outBitStream->Write(m_DirtyItemInfo || bIsInitialUpdate);
	if (m_DirtyItemInfo || bIsInitialUpdate) {
		outBitStream->Write(m_UgId);
		outBitStream->Write(m_UgModerationStatus);
		outBitStream->Write(!m_UgDescription.empty());
		if (!m_UgDescription.empty()){
			outBitStream->Write<uint32_t>(m_UgDescription.length());
			outBitStream->Write(reinterpret_cast<const char*>(m_UgDescription.c_str()), m_UgDescription.length() * sizeof(uint16_t));
		}
		m_DirtyItemInfo = false;
	}

}
