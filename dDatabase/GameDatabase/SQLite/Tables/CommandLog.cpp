#include "SQLiteDatabase.h"
#include "GeneralUtils.h"
#include <ctime>
#include "json.hpp"

void SQLiteDatabase::InsertSlashCommandUsage(const LWOOBJID characterId, const std::string_view command) {
	ExecuteInsert("INSERT INTO command_log (character_id, command, time) VALUES (?, ?, ?);", characterId, command, static_cast<int64_t>(std::time(nullptr)));
}

std::string SQLiteDatabase::GetCommandLogTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// A number in the search box also matches IDs exactly (-1 never matches)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	std::string baseQuery = "SELECT cl.id, cl.character_id, c.name as character_name, cl.command, cl.time FROM command_log cl LEFT JOIN charinfo c ON cl.character_id = c.id";
	std::string whereClause;
	if (!search.empty()) whereClause = " WHERE (c.name LIKE '%' || ? || '%' OR cl.command LIKE '%' || ? || '%' OR cl.character_id = ?)";

	std::string orderColumnName = "cl.id";
	switch (orderColumn) {
		case 0: orderColumnName = "cl.id"; break;
		case 1: orderColumnName = "c.name"; break;
		case 2: orderColumnName = "cl.command"; break;
		case 3: orderColumnName = "cl.time"; break;
	}
	// Rows with the same value (e.g. the same second) come in id order, the same on every database
	std::string orderClause = " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC") + ", cl.id" + (orderAsc ? " ASC" : " DESC");
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ? OFFSET ?;";
	// Without a name search or sort, page through the log first and join names for that page only; joining first
	// costs a character lookup for every skipped row, which made later pages of a big log slow
	if (search.empty() && orderColumn != 1) {
		mainQuery = "SELECT cl.id, cl.character_id, c.name as character_name, cl.command, cl.time FROM (SELECT * FROM command_log cl" + orderClause + " LIMIT ? OFFSET ?) cl "
			"LEFT JOIN charinfo c ON cl.character_id = c.id" + orderClause + ";";
	}

	auto [__, totalCountResult] = ExecuteSelect("SELECT COUNT(*) as count FROM command_log;");
	uint32_t totalRecords = totalCountResult.eof() ? 0 : totalCountResult.getIntField("count");

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto [___, filteredCountResult] = ExecuteSelect("SELECT COUNT(*) as count FROM command_log cl LEFT JOIN charinfo c ON cl.character_id = c.id WHERE (c.name LIKE '%' || ? || '%' OR cl.command LIKE '%' || ? || '%' OR cl.character_id = ?);", search, search, searchId);
		filteredRecords = filteredCountResult.eof() ? 0 : filteredCountResult.getIntField("count");
	}

	auto [stmt, result] = !search.empty()
		? ExecuteSelect(mainQuery, search, search, searchId, length, start)
		: ExecuteSelect(mainQuery, length, start);

	nlohmann::json dataArray = nlohmann::json::array();
	while (!result.eof()) {
		dataArray.push_back({
			{"id", result.getIntField("id")},
			{"character_name", result.getStringField("character_name")},
			{"command", result.getStringField("command")},
			{"character_id", std::to_string(result.getInt64Field("character_id"))},
			{"time", result.getInt64Field("time")} // 0: logged before times were recorded
		});
		result.nextRow();
	}
	return nlohmann::json({{"draw", 0}, {"recordsTotal", totalRecords}, {"recordsFiltered", filteredRecords}, {"data", dataArray}}).dump();
}
