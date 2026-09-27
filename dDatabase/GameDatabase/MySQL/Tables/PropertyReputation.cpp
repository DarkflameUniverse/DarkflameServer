#include "MySQLDatabase.h"

IPropertyReputation::VisitorHistory MySQLDatabase::GetPropertyVisitorHistory(LWOOBJID propertyId, uint32_t accountId, uint32_t day, uint32_t days) {
	VisitorHistory history;
	auto result = ExecuteSelect(
		"SELECT COALESCE(SUM(CASE WHEN day = ? THEN points ELSE 0 END), 0) AS today, "
		"COALESCE(SUM(CASE WHEN day < ? AND points > 0 THEN 1 ELSE 0 END), 0) AS previous_days "
		"FROM property_reputation_visits WHERE property_id = ? AND account_id = ? AND day >= ? AND day <= ?;",
		day, day, propertyId, accountId, day >= days ? day - days : 0, day);
	if (result->next()) {
		history.today = result->getInt64("today");
		history.previousDays = static_cast<uint32_t>(result->getInt64("previous_days"));
	}
	return history;
}

int64_t MySQLDatabase::GetPropertyReputationOnDay(LWOOBJID propertyId, uint32_t day) {
	auto result = ExecuteSelect("SELECT COALESCE(SUM(points), 0) AS points FROM property_reputation_visits WHERE property_id = ? AND day = ?;", propertyId, day);
	return result->next() ? result->getInt64("points") : 0;
}

void MySQLDatabase::AddPropertyReputation(LWOOBJID propertyId, uint32_t accountId, uint32_t day, int64_t points, int64_t seconds) {
	ExecuteInsert(
		"INSERT INTO property_reputation_visits (property_id, account_id, day, points, seconds) VALUES (?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE points = points + VALUES(points), seconds = seconds + VALUES(seconds);",
		propertyId, accountId, day, points, seconds);
	if (points > 0) ExecuteUpdate("UPDATE properties SET reputation = reputation + ? WHERE id = ?;", points, propertyId);
}

nlohmann::json MySQLDatabase::GetPropertyReputationDays(LWOOBJID propertyId, uint32_t fromDay) {
	auto result = ExecuteSelect(
		"SELECT day, COUNT(*) AS visitors, SUM(points) AS points, SUM(seconds) AS seconds FROM property_reputation_visits "
		"WHERE property_id = ? AND day >= ? GROUP BY day ORDER BY day DESC;", propertyId, fromDay);
	nlohmann::json days = nlohmann::json::array();
	while (result->next()) {
		days.push_back({ {"day", result->getInt("day")}, {"visitors", result->getInt64("visitors")}, {"points", result->getInt64("points")}, {"seconds", result->getInt64("seconds")} });
	}
	return days;
}
