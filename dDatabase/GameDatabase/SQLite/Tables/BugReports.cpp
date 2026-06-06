#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertNewBugReport(const IBugReports::Info& info) {
	ExecuteInsert("INSERT INTO `bug_reports`(body, client_version, other_player_id, selection, reporter_id) VALUES (?, ?, ?, ?, ?)",
		info.body, info.clientVersion, info.otherPlayer, info.selection, info.characterId);
}

#include "json.hpp"

std::string SQLiteDatabase::GetBugReportsTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// Build base query
	std::string baseQuery = "SELECT id, body, client_version, other_player_id, selection, submitted FROM bug_reports";
	std::string whereClause;
	std::string orderClause;

	// Add search filter if provided
	if (!search.empty()) {
		whereClause = " WHERE body LIKE '%' || ? || '%' OR other_player_id LIKE '%' || ? || '%'";
	}

	// Map column indices to database columns
	std::string orderColumnName = "id";
	switch (orderColumn) {
		case 0: orderColumnName = "id"; break;
		case 1: orderColumnName = "other_player_id"; break;
		case 2: orderColumnName = "client_version"; break;
		case 3: orderColumnName = "submitted"; break;
		default: orderColumnName = "id";
	}

	orderClause = " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC");

	// Build the main query
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ? OFFSET ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM bug_reports;";
	auto [__, totalCountResult] = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult.eof() ? 0 : totalCountResult.getIntField("count");

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM bug_reports WHERE body LIKE '%' || ? || '%' OR other_player_id LIKE '%' || ? || '%';";
		auto [___, filteredCountResult] = ExecuteSelect(filteredCountQuery, search, search);
		filteredRecords = filteredCountResult.eof() ? 0 : filteredCountResult.getIntField("count");
	}

	// Execute main query
	auto [stmt, result] = !search.empty() ? 
		ExecuteSelect(mainQuery, search, search, length, start) :
		ExecuteSelect(mainQuery, length, start);

	// Build response JSON
	nlohmann::json reportsArray = nlohmann::json::array();

	while (!result.eof()) {
		nlohmann::json report = {
			{"id", result.getIntField("id")},
			{"other_player_id", result.getStringField("other_player_id")},
			{"client_version", result.getStringField("client_version")},
			{"selection", result.getStringField("selection")},
			{"submitted", result.getStringField("submitted")},
			{"body", result.getStringField("body")}
		};
		reportsArray.push_back(report);
		result.nextRow();
	}

	nlohmann::json response = {
		{"draw", 0},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", reportsArray}
	};

	return response.dump();
}
