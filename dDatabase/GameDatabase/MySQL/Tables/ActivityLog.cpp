#include "MySQLDatabase.h"
#include "GeneralUtils.h"
#include "json.hpp"

void MySQLDatabase::UpdateActivityLog(const LWOOBJID characterId, const eActivityType activityType, const LWOMAPID mapId) {
	ExecuteInsert("INSERT INTO activity_log (character_id, activity, time, map_id) VALUES (?, ?, ?, ?);",
		characterId, static_cast<uint32_t>(activityType), static_cast<uint32_t>(time(NULL)), mapId);
}

std::string MySQLDatabase::GetActivityLogTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// A number in the search box also matches IDs exactly (-1 never matches)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	std::string baseQuery = "SELECT a.id, a.character_id, c.name as character_name, a.activity, a.time, a.map_id FROM activity_log a LEFT JOIN charinfo c ON a.character_id = c.id";
	std::string whereClause;
	if (!search.empty()) whereClause = " WHERE (c.name LIKE CONCAT('%', ?, '%') OR a.character_id = ?)";
	// The same search as the characters it matches: few characters are read through the character_id index instead of
	// joining every row of the log to a name to test it
	const std::string matchJoin = " JOIN (SELECT id FROM charinfo WHERE name LIKE CONCAT('%', ?, '%') UNION SELECT ?) m ON m.id = a.character_id";

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
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ?, ?;";
	// Without a name search or sort, page through the log first and join names for that page only; joining first
	// costs a character lookup for every skipped row, which made later pages of a big log slow
	if (search.empty() && orderColumn != 1) {
		mainQuery = "SELECT a.id, a.character_id, c.name as character_name, a.activity, a.time, a.map_id FROM (SELECT * FROM activity_log a" + orderClause + " LIMIT ?, ?) a "
			"LEFT JOIN charinfo c ON a.character_id = c.id" + orderClause + ";";
	} else if (search.empty()) {
		// Sorted by name, every row has to be joined to its name, but only the ids need sorting; the page's rows are read after
		mainQuery = "SELECT a.id, a.character_id, c.name as character_name, a.activity, a.time, a.map_id FROM activity_log a JOIN (SELECT a.id FROM activity_log a "
			"LEFT JOIN charinfo c ON a.character_id = c.id" + orderClause + " LIMIT ?, ?) p ON p.id = a.id LEFT JOIN charinfo c ON a.character_id = c.id" + orderClause + ";";
	}

	auto totalCountResult = ExecuteSelect("SELECT COUNT(*) as count FROM activity_log;");
	uint32_t totalRecords = totalCountResult->next() ? totalCountResult->getUInt("count") : 0;

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto filteredCountResult = ExecuteSelect("SELECT COUNT(*) as count FROM activity_log a" + matchJoin + ";", search, searchId);
		filteredRecords = filteredCountResult->next() ? filteredCountResult->getUInt("count") : 0;
		// When the matches are a small part of the log, sorting just them beats walking the log in order until a page of
		// them turns up (which reads nearly all of it when there are few or none)
		if (static_cast<uint64_t>(filteredRecords) * 20 < totalRecords) {
			mainQuery = "SELECT a.id, a.character_id, c.name as character_name, a.activity, a.time, a.map_id FROM activity_log a" + matchJoin +
				" LEFT JOIN charinfo c ON a.character_id = c.id" + orderClause + " LIMIT ?, ?;";
		}
	}

	auto result = !search.empty()
		? ExecuteSelect(mainQuery, search, searchId, start, length)
		: ExecuteSelect(mainQuery, start, length);

	nlohmann::json dataArray = nlohmann::json::array();
	while (result->next()) {
		dataArray.push_back({
			{"id", result->getUInt("id")},
			{"character_name", result->getString("character_name")},
			{"activity", result->getInt("activity")},
			{"time", result->getUInt64("time")},
			{"map_id", result->getInt("map_id")}
		});
	}
	return nlohmann::json({{"draw", 0}, {"recordsTotal", totalRecords}, {"recordsFiltered", filteredRecords}, {"data", dataArray}}).dump();
}

uint32_t MySQLDatabase::GetActivityLogCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM activity_log;");
	return res->next() ? res->getUInt("count") : 0;
}
