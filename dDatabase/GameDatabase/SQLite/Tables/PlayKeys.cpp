#include "SQLiteDatabase.h"

std::optional<bool> SQLiteDatabase::IsPlaykeyActive(const int32_t playkeyId) {
	auto [_, keyCheckRes] = ExecuteSelect("SELECT active FROM `play_keys` WHERE id=?", playkeyId);

	if (keyCheckRes.eof()) {
		return std::nullopt;
	}

	return keyCheckRes.getIntField("active");
}

#include "json.hpp"

std::string SQLiteDatabase::GetPlayKeysTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// Build base query
	std::string baseQuery = "SELECT id, key_string, key_uses, created_at, active FROM play_keys";
	std::string whereClause;
	std::string orderClause;

	// Add search filter if provided
	if (!search.empty()) {
		whereClause = " WHERE key_string LIKE '%' || ? || '%'";
	}

	// Map column indices to database columns
	std::string orderColumnName = "id";
	switch (orderColumn) {
		case 0: orderColumnName = "id"; break;
		case 1: orderColumnName = "key_string"; break;
		case 2: orderColumnName = "key_uses"; break;
		case 3: orderColumnName = "created_at"; break;
		case 4: orderColumnName = "active"; break;
		default: orderColumnName = "id";
	}

	orderClause = " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC");

	// Build the main query
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ? OFFSET ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM play_keys;";
	auto [__, totalCountResult] = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult.eof() ? 0 : totalCountResult.getIntField("count");

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM play_keys WHERE key_string LIKE '%' || ? || '%';";
		auto [___, filteredCountResult] = ExecuteSelect(filteredCountQuery, search);
		filteredRecords = filteredCountResult.eof() ? 0 : filteredCountResult.getIntField("count");
	}

	// Execute main query
	auto [stmt, result] = !search.empty() ? 
		ExecuteSelect(mainQuery, search, length, start) :
		ExecuteSelect(mainQuery, length, start);

	// Build response JSON
	nlohmann::json keysArray = nlohmann::json::array();

	while (!result.eof()) {
		nlohmann::json key = {
			{"id", result.getIntField("id")},
			{"key_string", result.getStringField("key_string")},
			{"key_uses", result.getIntField("key_uses")},
			{"created_at", result.getStringField("created_at")},
			{"active", result.getIntField("active")}
		};
		keysArray.push_back(key);
		result.nextRow();
	}

	nlohmann::json response = {
		{"draw", 0},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", keysArray}
	};

	return response.dump();
}
