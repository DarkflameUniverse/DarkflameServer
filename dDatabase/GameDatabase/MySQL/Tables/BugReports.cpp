#include "MySQLDatabase.h"

void MySQLDatabase::InsertNewBugReport(const IBugReports::Info& info) {
	ExecuteInsert("INSERT INTO `bug_reports`(body, client_version, other_player_id, selection, reporter_id) VALUES (?, ?, ?, ?, ?)",
		info.body, info.clientVersion, info.otherPlayer, info.selection, info.characterId);
}

#include "json.hpp"

std::string MySQLDatabase::GetBugReportsTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// Build base query
	std::string baseQuery = "SELECT id, body, client_version, other_player_id, selection, submitted FROM bug_reports";
	std::string whereClause;
	std::string orderClause;

	// Add search filter if provided
	if (!search.empty()) {
		whereClause = " WHERE body LIKE CONCAT('%', ?, '%') OR other_player_id LIKE CONCAT('%', ?, '%')";
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
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ?, ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM bug_reports;";
	auto totalCountResult = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult->next() ? totalCountResult->getUInt("count") : 0;

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM bug_reports WHERE body LIKE CONCAT('%', ?, '%') OR other_player_id LIKE CONCAT('%', ?, '%');";
		auto filteredCountResult = ExecuteSelect(filteredCountQuery, search, search);
		filteredRecords = filteredCountResult->next() ? filteredCountResult->getUInt("count") : 0;
	}

	// Execute main query
	std::unique_ptr<sql::ResultSet> result;
	if (!search.empty()) {
		result = ExecuteSelect(mainQuery, search, search, start, length);
	} else {
		result = ExecuteSelect(mainQuery, start, length);
	}

	// Build response JSON
	nlohmann::json reportsArray = nlohmann::json::array();

	while (result->next()) {
		nlohmann::json report = {
			{"id", result->getUInt("id")},
			{"other_player_id", result->getString("other_player_id")},
			{"client_version", result->getString("client_version")},
			{"selection", result->getString("selection")},
			{"submitted", result->getString("submitted")},
			{"body", result->getString("body")}
		};
		reportsArray.push_back(report);
	}

	nlohmann::json response = {
		{"draw", 0},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", reportsArray}
	};

	return response.dump();
}
