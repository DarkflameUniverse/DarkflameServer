#include "ItemComponent.h"

ItemComponent::ItemComponent(Entity* entity) : Component(entity) {
	m_UgcId = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");
	m_Description = GeneralUtils::ASCIIToUTF16(m_Parent->GetVar<std::string>(u"userModelDesc"));
	m_Dirty = false;
	m_ModerationStatus = 2;
	LOG("%s", m_Parent->GetVarAsString(u"userModelDesc").c_str());
}

void ItemComponent::Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {
	outBitStream.Write(m_Dirty || isConstruction);
	if (m_Dirty || isConstruction) {
		outBitStream.Write(m_UgcId);
		outBitStream.Write(m_ModerationStatus);
		outBitStream.Write(!m_Description.empty());
		if (!m_Description.empty()) {
			outBitStream.Write<uint32_t>(m_Description.size());
			outBitStream.Write(m_Description);
		}
		if (!isConstruction) m_Dirty = false;
	}
}

void ItemComponent::UpdateDescription(const std::u16string& description) {
	if (m_Description == description) return;
	m_Dirty = true;
	m_Description = description;
}
