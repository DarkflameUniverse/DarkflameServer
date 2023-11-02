#include "DonationVendorComponent.h"
#include "Database.h"

DonationVendorComponent::DonationVendorComponent(Entity* parent) : VendorComponent(parent) {
	//LoadConfigData
	m_PercentComplete = 0.0;
	m_TotalDonated = 0;
	m_TotalRemaining = 0;

	// custom attribute to calculate other values
	m_Goal = m_Parent->GetVar<int32_t>(u"donationGoal");
	if (m_Goal == 0) m_Goal = INT32_MAX;

	// Default to the nexus tower jawbox activity and setup settings
	m_ActivityId = m_Parent->GetVar<uint32_t>(u"activityID");
	if ((m_ActivityId == 0) || (m_ActivityId == 117)) {
		m_ActivityId = 117;
		m_PercentComplete = 1.0;
		m_TotalDonated = INT32_MAX;
		m_TotalRemaining = 0;
		m_Goal = INT32_MAX;
		return;
	}

	std::unique_ptr<sql::PreparedStatement> query(Database::Get()->CreatePreppedStmt("SELECT SUM(primaryScore) as donation_total FROM leaderboard WHERE game_id = ?;"));
	query->setInt(1, m_ActivityId);
	std::unique_ptr<sql::ResultSet> donation_total(query->executeQuery());
	if (donation_total->next()) m_TotalDonated = donation_total->getInt("donation_total");
	m_TotalRemaining = m_Goal - m_TotalDonated;
	m_PercentComplete = m_TotalDonated/static_cast<float>(m_Goal);
}

void DonationVendorComponent::SubmitDonation(uint32_t count) {
	if (count <= 0 && ((m_TotalDonated + count) > 0)) return;
	m_TotalDonated += count;
	m_TotalRemaining = m_Goal - m_TotalDonated;
	m_PercentComplete = m_TotalDonated/static_cast<float>(m_Goal);
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
