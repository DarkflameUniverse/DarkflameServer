#include "SQLiteDatabase.h"
#include "GeneralUtils.h"
#include "json.hpp"

void SQLiteDatabase::UpdateActivityLog(const LWOOBJID characterId, const eActivityType activityType, const LWOMAPID mapId) {
	ExecuteInsert("INSERT INTO activity_log (character_id, activity, time, map_id) VALUES (?, ?, ?, ?);",
		characterId, static_cast<uint32_t>(activityType), static_cast<uint32_t>(time(NULL)), mapId);
}

std::string SQLiteDatabase::GetActivityLogTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// A number in the search box also matches IDs exactly (-1 never matches)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	std::string baseQuery = "SELECT a.id, a.character_id, c.name as character_name, a.activity, a.time, a.map_id FROM activity_log a LEFT JOIN charinfo c ON a.character_id = c.id";
	std::string whereClause;
	if (!search.empty()) whereClause = " WHERE (c.name LIKE '%' || ? || '%' OR a.character_id = ?)";

	std::string orderColumnName = "a.id";
	switch (orderColumn) {
		case 0: orderColumnName = "a.id"; break;
		case 1: orderColumnName = "c.name"; break;
		case 2: orderColumnName = "a.activity"; break;
		case 3: orderColumnName = "a.time"; break;
		case 4: orderColumnName = "a.map_id"; break;
	}
	// Rows with the same value (e.g. the same second) come in id order, the same on every database
	std::string orderClause = " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC") + ", a.id" + (orderAsc ? " ASC" : " DESC");
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ? OFFSET ?;";
	// Without a name search or sort, page through the log first and join names for that page only; joining first
	// costs a character lookup for every skipped row, which made later pages of a big log slow
	if (search.empty() && orderColumn != 1) {
		mainQuery = "SELECT a.id, a.character_id, c.name as character_name, a.activity, a.time, a.map_id FROM (SELECT * FROM activity_log a" + orderClause + " LIMIT ? OFFSET ?) a "
			"LEFT JOIN charinfo c ON a.character_id = c.id" + orderClause + ";";
	}

	auto [__, totalCountResult] = ExecuteSelect("SELECT COUNT(*) as count FROM activity_log;");
	uint32_t totalRecords = totalCountResult.eof() ? 0 : totalCountResult.getIntField("count");

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto [___, filteredCountResult] = ExecuteSelect("SELECT COUNT(*) as count FROM activity_log a LEFT JOIN charinfo c ON a.character_id = c.id WHERE (c.name LIKE '%' || ? || '%' OR a.character_id = ?);", search, searchId);
		filteredRecords = filteredCountResult.eof() ? 0 : filteredCountResult.getIntField("count");
	}

	auto [stmt, result] = !search.empty()
		? ExecuteSelect(mainQuery, search, searchId, length, start)
		: ExecuteSelect(mainQuery, length, start);

	nlohmann::json dataArray = nlohmann::json::array();
	while (!result.eof()) {
		dataArray.push_back({
			{"id", result.getIntField("id")},
			{"character_name", result.getStringField("character_name")},
			{"activity", result.getIntField("activity")},
			{"time", result.getInt64Field("time")},
			{"map_id", result.getIntField("map_id")}
		});
		result.nextRow();
	}
	return nlohmann::json({{"draw", 0}, {"recordsTotal", totalRecords}, {"recordsFiltered", filteredRecords}, {"data", dataArray}}).dump();
}

uint32_t SQLiteDatabase::GetActivityLogCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM activity_log;");
	if (res.eof()) return 0;
	return res.getIntField("count");
}
