#include "SQLiteDatabase.h"
#include "GeneralUtils.h"
#include "json.hpp"
#include <ctime>

void SQLiteDatabase::InsertAuditLog(uint32_t accountId, const std::string_view accountName, const std::string_view action, const std::string_view description,
	uint32_t targetAccountId, LWOOBJID targetCharacterId) {
	ExecuteInsert("INSERT INTO audit_log (account_id, account_name, action, description, timestamp, target_account_id, target_character_id) VALUES (?, ?, ?, ?, ?, ?, ?);",
		accountId, accountName, action, description, static_cast<uint64_t>(std::time(nullptr)), targetAccountId, targetCharacterId);
}

std::string SQLiteDatabase::GetAuditLogTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// A number in the search box also matches IDs exactly (-1 never matches)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	std::string baseQuery = "SELECT id, account_id, account_name, action, description, timestamp, target_account_id, target_character_id, "
		"(SELECT name FROM accounts WHERE accounts.id = audit_log.target_account_id) AS target_account_name, "
		"(SELECT name FROM charinfo WHERE charinfo.id = audit_log.target_character_id) AS target_character_name FROM audit_log";
	std::string whereClause;

	if (!search.empty()) {
		whereClause = " WHERE (action LIKE '%' || ? || '%' OR account_name LIKE '%' || ? || '%' OR description LIKE '%' || ? || '%' OR account_id = ? OR target_account_id = ? OR target_character_id = ?)";
	}

	std::string orderColumnName = "id";
	switch (orderColumn) {
		case 0: orderColumnName = "id"; break;
		case 1: orderColumnName = "account_name"; break;
		case 2: orderColumnName = "action"; break;
		case 5: orderColumnName = "timestamp"; break; // columns 3 and 4 (about, description) don't sort
		default: orderColumnName = "id";
	}

	// Rows with the same value (e.g. the same second) come in id order, the same on every database
	std::string orderClause = " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC") + ", id" + (orderAsc ? " ASC" : " DESC");

	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ? OFFSET ?;";

	auto [__, totalCountResult] = ExecuteSelect("SELECT COUNT(*) as count FROM audit_log;");
	uint32_t totalRecords = totalCountResult.eof() ? 0 : totalCountResult.getIntField("count");

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto [___, filteredCountResult] = ExecuteSelect("SELECT COUNT(*) as count FROM audit_log WHERE (action LIKE '%' || ? || '%' OR account_name LIKE '%' || ? || '%' OR description LIKE '%' || ? || '%' OR account_id = ? OR target_account_id = ? OR target_character_id = ?);", search, search, search, searchId, searchId, searchId);
		filteredRecords = filteredCountResult.eof() ? 0 : filteredCountResult.getIntField("count");
	}

	auto [stmt, result] = !search.empty()
		? ExecuteSelect(mainQuery, search, search, search, searchId, searchId, searchId, length, start)
		: ExecuteSelect(mainQuery, length, start);

	nlohmann::json dataArray = nlohmann::json::array();

	while (!result.eof()) {
		dataArray.push_back({
			{"id", result.getIntField("id")},
			{"account_name", result.getStringField("account_name")},
			{"action", result.getStringField("action")},
			{"description", result.getStringField("description")},
			{"timestamp", result.getInt64Field("timestamp")},
			{"target_account_id", result.getIntField("target_account_id")},
			{"target_account_name", std::string(result.fieldIsNull("target_account_name") ? "" : result.getStringField("target_account_name"))},
			{"target_character_id", std::to_string(result.getInt64Field("target_character_id"))},
			{"target_character_name", std::string(result.fieldIsNull("target_character_name") ? "" : result.getStringField("target_character_name"))}
		});
		result.nextRow();
	}

	nlohmann::json response = {
		{"draw", 0},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", dataArray}
	};

	return response.dump();
}
