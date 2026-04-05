#include "MySQLDatabase.h"

std::vector<IPropertyReputationContribution::ContributionInfo> MySQLDatabase::GetPropertyReputationContributions(
	const LWOOBJID propertyId, const std::string& date) {
	auto result = ExecuteSelect(
		"SELECT player_id, reputation_gained FROM property_reputation_contribution WHERE property_id = ? AND contribution_date = ?;",
		propertyId, date);

	std::vector<IPropertyReputationContribution::ContributionInfo> contributions;
	while (result->next()) {
		IPropertyReputationContribution::ContributionInfo info;
		info.playerId = result->getUInt64("player_id");
		info.reputationGained = static_cast<uint32_t>(result->getUInt("reputation_gained"));
		contributions.push_back(info);
	}
	return contributions;
}

void MySQLDatabase::UpdatePropertyReputationContribution(
	const LWOOBJID propertyId, const LWOOBJID playerId,
	const std::string& date, const uint32_t reputationGained) {
	ExecuteInsert(
		"INSERT INTO property_reputation_contribution (property_id, player_id, contribution_date, reputation_gained) "
		"VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE reputation_gained = ?;",
		propertyId, playerId, date, reputationGained, reputationGained);
}

void MySQLDatabase::UpdatePropertyReputation(const LWOOBJID propertyId, const uint32_t reputation) {
	ExecuteUpdate("UPDATE properties SET reputation = ? WHERE id = ?;", reputation, propertyId);
}
