#include "DonationVendorComponent.h"

DonationVendorComponent::DonationVendorComponent(Entity* parent) : VendorComponent(parent) {
	Game::logger->Log("DonationVendorComponent", "loading donation ventory %llu with lot %i", m_Parent->GetObjectID(), m_Parent->GetLOT());
	m_PercentComplete = 0.691;
	m_TotalDonated = 42069;
	m_TotalRemaining = 696969;
	//LoadConfigData
	m_ActivityId = m_Parent->GetVar<uint32_t>(u"activityID");
	//LoadVendorConfigData

	//Load event data?
}

void DonationVendorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	VendorComponent::Serialize(outBitStream, bIsInitialUpdate, flags);
	outBitStream->Write(bIsInitialUpdate || m_DirtyDonationVendor);
	if (bIsInitialUpdate || m_DirtyDonationVendor) {
		outBitStream->Write(m_PercentComplete);
		outBitStream->Write(m_TotalDonated);
		outBitStream->Write(m_TotalRemaining);
		if (!bIsInitialUpdate) m_DirtyDonationVendor = false;
	}
}
