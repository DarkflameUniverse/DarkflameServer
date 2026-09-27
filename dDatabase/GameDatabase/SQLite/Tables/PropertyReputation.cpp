#include "SQLiteDatabase.h"

IPropertyReputation::VisitorHistory SQLiteDatabase::GetPropertyVisitorHistory(LWOOBJID propertyId, uint32_t accountId, uint32_t day, uint32_t days) {
	VisitorHistory history;
	auto [_, result] = ExecuteSelect(
		"SELECT COALESCE(SUM(CASE WHEN day = ? THEN points ELSE 0 END), 0) AS today, "
		"COALESCE(SUM(CASE WHEN day < ? AND points > 0 THEN 1 ELSE 0 END), 0) AS previous_days "
		"FROM property_reputation_visits WHERE property_id = ? AND account_id = ? AND day >= ? AND day <= ?;",
		day, day, propertyId, accountId, day >= days ? day - days : 0, day);
	if (!result.eof()) {
		history.today = result.getInt64Field("today");
		history.previousDays = static_cast<uint32_t>(result.getInt64Field("previous_days"));
	}
	return history;
}

int64_t SQLiteDatabase::GetPropertyReputationOnDay(LWOOBJID propertyId, uint32_t day) {
	auto [_, result] = ExecuteSelect("SELECT COALESCE(SUM(points), 0) AS points FROM property_reputation_visits WHERE property_id = ? AND day = ?;", propertyId, day);
	return result.eof() ? 0 : result.getInt64Field("points");
}

void SQLiteDatabase::AddPropertyReputation(LWOOBJID propertyId, uint32_t accountId, uint32_t day, int64_t points, int64_t seconds) {
	ExecuteInsert(
		"INSERT INTO property_reputation_visits (property_id, account_id, day, points, seconds) VALUES (?, ?, ?, ?, ?) "
		"ON CONFLICT(property_id, account_id, day) DO UPDATE SET points = points + excluded.points, seconds = seconds + excluded.seconds;",
		propertyId, accountId, day, points, seconds);
	if (points > 0) ExecuteUpdate("UPDATE properties SET reputation = reputation + ? WHERE id = ?;", points, propertyId);
}

nlohmann::json SQLiteDatabase::GetPropertyReputationDays(LWOOBJID propertyId, uint32_t fromDay) {
	auto [_, result] = ExecuteSelect(
		"SELECT day, COUNT(*) AS visitors, SUM(points) AS points, SUM(seconds) AS seconds FROM property_reputation_visits "
		"WHERE property_id = ? AND day >= ? GROUP BY day ORDER BY day DESC;", propertyId, fromDay);
	nlohmann::json days = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		days.push_back({ {"day", result.getIntField("day")}, {"visitors", result.getInt64Field("visitors")}, {"points", result.getInt64Field("points")}, {"seconds", result.getInt64Field("seconds")} });
	}
	return days;
}
