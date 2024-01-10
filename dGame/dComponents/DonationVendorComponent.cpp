#include "DonationVendorComponent.h"
#include "Database.h"

DonationVendorComponent::DonationVendorComponent(Entity* parent) : VendorComponent(parent) {
	//LoadConfigData
	m_PercentComplete = 0.0;
	m_TotalDonated = 0;
	m_TotalRemaining = 0;

	auto* const parentEntity = Game::entityManager->GetEntity(m_Parent);

	// custom attribute to calculate other values
	m_Goal = parentEntity->GetVar<int32_t>(u"donationGoal");
	if (m_Goal == 0) m_Goal = INT32_MAX;

	// Default to the nexus tower jawbox activity and setup settings
	m_ActivityId = parentEntity->GetVar<uint32_t>(u"activityID");
	if ((m_ActivityId == 0) || (m_ActivityId == 117)) {
		m_ActivityId = 117;
		m_PercentComplete = 1.0;
		m_TotalDonated = INT32_MAX;
		m_TotalRemaining = 0;
		m_Goal = INT32_MAX;
		return;
	}

	auto donationTotal = Database::Get()->GetDonationTotal(m_ActivityId);
	if (donationTotal) m_TotalDonated = donationTotal.value();
	m_TotalRemaining = m_Goal - m_TotalDonated;
	m_PercentComplete = m_TotalDonated / static_cast<float>(m_Goal);
}

void DonationVendorComponent::SubmitDonation(uint32_t count) {
	if (count <= 0 && ((m_TotalDonated + count) > 0)) return;
	m_TotalDonated += count;
	m_TotalRemaining = m_Goal - m_TotalDonated;
	m_PercentComplete = m_TotalDonated / static_cast<float>(m_Goal);
	m_DirtyDonationVendor = true;
}

void DonationVendorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	VendorComponent::Serialize(outBitStream, bIsInitialUpdate);
	outBitStream->Write(bIsInitialUpdate || m_DirtyDonationVendor);
	if (bIsInitialUpdate || m_DirtyDonationVendor) {
		outBitStream->Write(m_PercentComplete);
		outBitStream->Write(m_TotalDonated);
		outBitStream->Write(m_TotalRemaining);
		if (!bIsInitialUpdate) m_DirtyDonationVendor = false;
	}
}
