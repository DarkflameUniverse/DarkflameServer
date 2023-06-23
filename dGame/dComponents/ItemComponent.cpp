#include "ItemComponent.h"
#include "Entity.h"
#include "eUgcModerationStatus.h"


ItemComponent::ItemComponent(Entity* parent) : Component(parent) {
	m_Parent = parent;

	m_DirtyItemInfo = false;

	m_UgId = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");
	if (m_UgId == LWOOBJID_EMPTY) m_UgId = m_Parent->GetObjectID();

	m_UgModerationStatus = eUgcModerationStatus::NoStatus;

	m_UgDescription = u"";
}

void ItemComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {

	outBitStream->Write(m_DirtyItemInfo || bIsInitialUpdate);
	if (m_DirtyItemInfo || bIsInitialUpdate){
		outBitStream->Write(m_UgId);
		outBitStream->Write(m_UgModerationStatus);
		outBitStream->Write(m_UgDescription != u"");
		if (m_UgDescription != u""){
			outBitStream->Write<uint32_t>(m_UgDescription.length());
			for (uint16_t character : m_UgDescription) outBitStream->Write(character);
		}
		m_DirtyItemInfo = false;
	}

}
