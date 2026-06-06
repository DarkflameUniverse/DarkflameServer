#include "MySQLDatabase.h"

std::optional<bool> MySQLDatabase::IsPlaykeyActive(const int32_t playkeyId) {
	auto keyCheckRes = ExecuteSelect("SELECT active FROM `play_keys` WHERE id=?", playkeyId);

	if (!keyCheckRes->next()) {
		return std::nullopt;
	}

	return keyCheckRes->getBoolean("active");
}

#include "json.hpp"

std::string MySQLDatabase::GetPlayKeysTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// Build base query
	std::string baseQuery = "SELECT id, key_string, key_uses, created_at, active FROM play_keys";
	std::string whereClause;
	std::string orderClause;

	// Add search filter if provided
	if (!search.empty()) {
		whereClause = " WHERE key_string LIKE CONCAT('%', ?, '%')";
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
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ?, ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM play_keys;";
	auto totalCountResult = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult->next() ? totalCountResult->getUInt("count") : 0;

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM play_keys WHERE key_string LIKE CONCAT('%', ?, '%');";
		auto filteredCountResult = ExecuteSelect(filteredCountQuery, search);
		filteredRecords = filteredCountResult->next() ? filteredCountResult->getUInt("count") : 0;
	}

	// Execute main query
	std::unique_ptr<sql::ResultSet> result;
	if (!search.empty()) {
		result = ExecuteSelect(mainQuery, search, start, length);
	} else {
		result = ExecuteSelect(mainQuery, start, length);
	}

	// Build response JSON
	nlohmann::json keysArray = nlohmann::json::array();

	while (result->next()) {
		nlohmann::json key = {
			{"id", result->getUInt("id")},
			{"key_string", result->getString("key_string")},
			{"key_uses", result->getUInt("key_uses")},
			{"created_at", result->getString("created_at")},
			{"active", result->getBoolean("active")}
		};
		keysArray.push_back(key);
	}

	nlohmann::json response = {
		{"draw", 0},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", keysArray}
	};

	return response.dump();
}
