#include "ModelComponent.h"
#include "Entity.h"

ModelComponent::ModelComponent(Entity* parent) : Component(parent) {
	m_OriginalPosition = m_Parent->GetDefaultPosition();
	m_OriginalRotation = m_Parent->GetDefaultRotation();
	m_Description = u"";
	m_Name = u"";
	m_DescriptionStatus = ModerationStatus::NoStatus;
	m_NameStatus = ModerationStatus::NoStatus;

	m_userModelID = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");
}

void ModelComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	// ItemComponent Serialization.  Pets do not get this serialization.
	if (!m_Parent->HasComponent(COMPONENT_TYPE_PET)) {
		outBitStream->Write1();
		outBitStream->Write<LWOOBJID>(m_userModelID != LWOOBJID_EMPTY ? m_userModelID : m_Parent->GetObjectID());
		outBitStream->Write<ModerationStatus>(m_DescriptionStatus);
		outBitStream->Write(m_Description.size() > 0);
		if (m_Description.size() > 0) {
			outBitStream->Write(static_cast<uint32_t>(m_Description.size()));
			for (auto character : m_Description) outBitStream->Write(character);
		}
	}

	//actual model component:
	outBitStream->Write1(); // Yes we are writing model info
	outBitStream->Write0(); // Is pickable
	outBitStream->Write<uint32_t>(2); // Physics type
	outBitStream->Write(m_OriginalPosition); // Original position
	outBitStream->Write(m_OriginalRotation); // Original rotation

	outBitStream->Write1(); // We are writing behavior info
	outBitStream->Write<uint32_t>(0); // Number of behaviors
	outBitStream->Write1(); // Is this model paused
	if (bIsInitialUpdate) outBitStream->Write0(); // We are not writing model editing info
}
void ModelComponent::SetDescription(const std::u16string& description) {
	this->m_Description = description;
	m_DescriptionStatus = ModerationStatus::NoStatus;
	m_Parent->SetVar<std::u16string>(u"userModelDesc", description);
}

void ModelComponent::SetName(const std::u16string& name) {
	this->m_Name = name;
	m_NameStatus = ModerationStatus::NoStatus;
	m_Parent->SetVar<std::u16string>(u"userModelName", name);
}
