#include "SQLiteDatabase.h"

void SQLiteDatabase::UpdateActivityLog(const LWOOBJID characterId, const eActivityType activityType, const LWOMAPID mapId) {
	ExecuteInsert("INSERT INTO activity_log (character_id, activity, time, map_id) VALUES (?, ?, ?, ?);",
		characterId, static_cast<uint32_t>(activityType), static_cast<uint32_t>(time(NULL)), mapId);
}

std::vector<IActivityLog::Entry> SQLiteDatabase::GetRecentActivity(const uint32_t limit) {
	std::vector<IActivityLog::Entry> out;

	auto [stmt, res] = ExecuteSelect("SELECT character_id, activity, time, map_id FROM activity_log ORDER BY time DESC LIMIT ?;", limit);

	while (!res.eof()) {
		IActivityLog::Entry e;
		e.characterId = static_cast<LWOOBJID>(res.getInt64Field("character_id"));
		e.activity = static_cast<eActivityType>(res.getIntField("activity"));
		e.timestamp = static_cast<uint32_t>(res.getIntField("time"));
		e.mapId = static_cast<LWOMAPID>(res.getIntField("map_id"));
		out.push_back(e);
		res.nextRow();
	}

	return out;
}

uint32_t SQLiteDatabase::GetActivityLogCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM activity_log;");
	if (res.eof()) return 0;
	return res.getIntField("count");
}

std::vector<IActivityLog::Entry> SQLiteDatabase::GetActivityLogPaginated(
	uint32_t offset,
	uint32_t limit,
	const std::string& orderColumn,
	const std::string& orderDir
) {
	std::vector<IActivityLog::Entry> out;

	// Validate orderColumn to prevent SQL injection
	std::string validColumn = "time";
	if (orderColumn == "character_id" || orderColumn == "activity" || orderColumn == "map_id" || orderColumn == "time") {
		validColumn = orderColumn;
	}

	// Validate orderDir
	std::string validDir = (orderDir == "ASC" || orderDir == "asc") ? "ASC" : "DESC";

	// Build query - can't use prepared statement for ORDER BY clause
	std::string query = "SELECT character_id, activity, time, map_id FROM activity_log ORDER BY " +
		validColumn + " " + validDir + " LIMIT ? OFFSET ?;";

	auto [stmt, res] = ExecuteSelect(query, limit, offset);

	while (!res.eof()) {
		IActivityLog::Entry e;
		e.characterId = static_cast<LWOOBJID>(res.getInt64Field("character_id"));
		e.activity = static_cast<eActivityType>(res.getIntField("activity"));
		e.timestamp = static_cast<uint32_t>(res.getIntField("time"));
		e.mapId = static_cast<LWOMAPID>(res.getIntField("map_id"));
		out.push_back(e);
		res.nextRow();
	}

	return out;
}
