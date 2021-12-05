#include "ModelComponent.h"
#include "Entity.h"

ModelComponent::ModelComponent(uint32_t componentID, Entity* parent) : Component(parent)
{
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();

	m_userModelID = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");

	/*
	for (auto set : m_Parent->GetInfo().settings) {
		if (set && set->GetKey() == u"userModelID") {
			m_userModelID = std::stoull(set->GetValueAsString());
		}
	}
	*/
}

ModelComponent::~ModelComponent() {
}

void ModelComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	//item component:
	outBitStream->Write1();
	outBitStream->Write<LWOOBJID>(m_userModelID);
	outBitStream->Write<int>(0);
	outBitStream->Write0();
	
	//actual model component:
	outBitStream->Write1(); //yes we are writing model info
	outBitStream->Write0(); //??
	outBitStream->Write<int>(2); //model type, always 2 for BBB

	outBitStream->Write(m_Position);
	outBitStream->Write(m_Rotation);

	outBitStream->Write1(); //second data flag, all unknown. Maybe skip?
	outBitStream->Write<int>(0);
	outBitStream->Write1();
	outBitStream->Write0();
}
