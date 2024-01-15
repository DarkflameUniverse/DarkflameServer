#include "MySQLDatabase.h"

std::optional<uint32_t> MySQLDatabase::GetDonationTotal(const uint32_t activityId) {
	auto donation_total = ExecuteSelect("SELECT SUM(primaryScore) as donation_total FROM leaderboard WHERE game_id = ?;", activityId);

	if (!donation_total->next()) {
		return std::nullopt;
	}

	return donation_total->getUInt("donation_total");
}
