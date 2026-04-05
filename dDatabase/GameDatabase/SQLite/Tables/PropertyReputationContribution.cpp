#include "SQLiteDatabase.h"

std::vector<IPropertyReputationContribution::ContributionInfo> SQLiteDatabase::GetPropertyReputationContributions(
	const LWOOBJID propertyId, const std::string& date) {
	auto [_, result] = ExecuteSelect(
		"SELECT player_id, reputation_gained FROM property_reputation_contribution WHERE property_id = ? AND contribution_date = ?;",
		propertyId, date);

	std::vector<IPropertyReputationContribution::ContributionInfo> contributions;
	while (!result.eof()) {
		IPropertyReputationContribution::ContributionInfo info;
		info.playerId = result.getInt64Field("player_id");
		info.reputationGained = static_cast<uint32_t>(result.getIntField("reputation_gained"));
		contributions.push_back(info);
		result.nextRow();
	}
	return contributions;
}

void SQLiteDatabase::UpdatePropertyReputationContribution(
	const LWOOBJID propertyId, const LWOOBJID playerId,
	const std::string& date, const uint32_t reputationGained) {
	ExecuteInsert(
		"INSERT OR REPLACE INTO property_reputation_contribution (property_id, player_id, contribution_date, reputation_gained) "
		"VALUES (?, ?, ?, ?);",
		propertyId, playerId, date, reputationGained);
}

void SQLiteDatabase::UpdatePropertyReputation(const LWOOBJID propertyId, const uint32_t reputation) {
	ExecuteUpdate("UPDATE properties SET reputation = ? WHERE id = ?;", reputation, propertyId);
}
