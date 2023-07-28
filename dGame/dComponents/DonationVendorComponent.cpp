#include "DonationVendorComponent.h"
#include "Database.h"

DonationVendorComponent::DonationVendorComponent(Entity* parent) : VendorComponent(parent) {
	//LoadConfigData
	m_ActivityId = m_Parent->GetVar<uint32_t>(u"activityID");
	if (m_ActivityId == 0) m_ActivityId = 117; // Default to the nexus tower jawbox activity

	m_PercentComplete = 0.0;
	m_TotalDonated = 0;
	m_TotalRemaining = 0;
	m_Goal = 10000;

	std::unique_ptr<sql::PreparedStatement> query(Database::CreatePreppedStmt("SELECT SUM(primaryScore) as donation_total FROM leaderboard WHERE game_id = ?;"));
	query->setInt(1, m_ActivityId);
	std::unique_ptr<sql::ResultSet> donation_total(query->executeQuery());
	if (donation_total->next()) m_TotalDonated = donation_total->getInt("donation_total");
	m_TotalRemaining = m_Goal - m_TotalDonated;
	m_PercentComplete = m_TotalDonated/static_cast<float>(m_Goal);
	Game::logger->Log("DonationVendorComponent", "total donated: %i total remaining: %i per complete %f", m_TotalDonated, m_TotalRemaining, m_PercentComplete);
}

void DonationVendorComponent::SubmitDonation(uint32_t count) {
	if (count == 0) return;
	m_TotalDonated += count;
	m_TotalRemaining = m_Goal - m_TotalDonated;
	m_PercentComplete = m_TotalDonated/static_cast<float>(m_Goal);
	m_DirtyDonationVendor = true;
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
