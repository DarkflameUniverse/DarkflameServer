#include "DonationVendorComponent.h"

DonationVendorComponent::DonationVendorComponent(Entity* parent) : VendorComponent(parent) {
	m_PercentComplete = 0.0;
	m_TotalDonated = 0;
	m_TotalRemaining = 0;
}



void DonationVendorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	VendorComponent::Serialize(outBitStream, bIsInitialUpdate, flags);
	outBitStream->Write(bIsInitialUpdate || m_DirtyDonationVendor);
	if (bIsInitialUpdate || m_DirtyDonationVendor) {
		outBitStream->Write(m_PercentComplete);
		outBitStream->Write(m_TotalDonated);
		outBitStream->Write(m_TotalRemaining);
	}
}

void DonationVendorComponent::LoadConfigData() {
	m_ActivityId = m_ParentEntity->GetVar<uint32_t>(u"activityID");
	VendorComponent::LoadConfigData();
}
