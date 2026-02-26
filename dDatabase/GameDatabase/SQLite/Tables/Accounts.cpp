#include "SQLiteDatabase.h"

#include "eGameMasterLevel.h"
#include "Database.h"
#include "json.hpp"

std::optional<IAccounts::Info> SQLiteDatabase::GetAccountInfo(const std::string_view username) {
	auto [_, result] = ExecuteSelect("SELECT * FROM accounts WHERE name = ? LIMIT 1", username);

	if (result.eof()) {
		return std::nullopt;
	}

	IAccounts::Info toReturn;
	toReturn.id = result.getIntField("id");
	toReturn.maxGmLevel = static_cast<eGameMasterLevel>(result.getIntField("gm_level"));
	toReturn.bcryptPassword = result.getStringField("password");
	toReturn.banned = result.getIntField("banned");
	toReturn.locked = result.getIntField("locked");
	toReturn.playKeyId = result.getIntField("play_key_id");
	toReturn.muteExpire = static_cast<uint64_t>(result.getInt64Field("mute_expire"));

	return toReturn;
}

void SQLiteDatabase::UpdateAccountUnmuteTime(const uint32_t accountId, const uint64_t timeToUnmute) {
	ExecuteUpdate("UPDATE accounts SET mute_expire = ? WHERE id = ?;", timeToUnmute, accountId);
}

void SQLiteDatabase::UpdateAccountBan(const uint32_t accountId, const bool banned) {
	ExecuteUpdate("UPDATE accounts SET banned = ? WHERE id = ?;", banned, accountId);
}

void SQLiteDatabase::UpdateAccountPassword(const uint32_t accountId, const std::string_view bcryptpassword) {
	ExecuteUpdate("UPDATE accounts SET password = ? WHERE id = ?;", bcryptpassword, accountId);
}

void SQLiteDatabase::InsertNewAccount(const std::string_view username, const std::string_view bcryptpassword) {
	ExecuteInsert("INSERT INTO accounts (name, password, gm_level) VALUES (?, ?, ?);", username, bcryptpassword, static_cast<int32_t>(eGameMasterLevel::OPERATOR));
}

void SQLiteDatabase::UpdateAccountGmLevel(const uint32_t accountId, const eGameMasterLevel gmLevel) {
	ExecuteUpdate("UPDATE accounts SET gm_level = ? WHERE id = ?;", static_cast<int32_t>(gmLevel), accountId);
}

uint32_t SQLiteDatabase::GetAccountCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM accounts;");
	if (res.eof()) return 0;

	return res.getIntField("count");
}
void SQLiteDatabase::RecordFailedAttempt(const uint32_t accountId) {
	ExecuteUpdate("UPDATE accounts SET failed_attempts = failed_attempts + 1 WHERE id = ?;", accountId);
}

void SQLiteDatabase::ClearFailedAttempts(const uint32_t accountId) {
	ExecuteUpdate("UPDATE accounts SET failed_attempts = 0, lockout_time = NULL, last_login = CURRENT_TIMESTAMP WHERE id = ?;", accountId);
}

void SQLiteDatabase::SetLockout(const uint32_t accountId, const int64_t lockoutUntil) {
	ExecuteUpdate("UPDATE accounts SET lockout_time = datetime(?, 'unixepoch') WHERE id = ?;", lockoutUntil, accountId);
}

bool SQLiteDatabase::IsLockedOut(const uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT lockout_time FROM accounts WHERE id = ?;", accountId);
	if (result.eof()) {
		return false;
	}

	const char* lockoutTime = result.getStringField("lockout_time");
	if (lockoutTime == nullptr || strlen(lockoutTime) == 0 || strcmp(lockoutTime, "0") == 0) {
		return false;
	}

	// If lockout_time is set and in the future, account is locked
	// For now, simplified check - if lockout_time exists, it's locked
	return true;
}

uint8_t SQLiteDatabase::GetFailedAttempts(const uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT failed_attempts FROM accounts WHERE id = ?;", accountId);
	if (result.eof()) {
		return 0;
	}

	return result.getIntField("failed_attempts");
}

nlohmann::json SQLiteDatabase::GetAccountsTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// Build base query
	std::string baseQuery = "SELECT id, name, banned, locked, gm_level, mute_expire, created_at FROM accounts";
	std::string whereClause;
	std::string orderClause;

	// Add search filter if provided
	if (!search.empty()) {
		whereClause = " WHERE name LIKE '%' || ? || '%'";
	}

	// Map column indices to database columns
	std::string orderColumnName = "id";
	switch (orderColumn) {
		case 0: orderColumnName = "id"; break;
		case 1: orderColumnName = "name"; break;
		case 2: orderColumnName = "banned"; break;
		case 3: orderColumnName = "locked"; break;
		case 4: orderColumnName = "gm_level"; break;
		case 5: orderColumnName = "mute_expire"; break;
		case 6: orderColumnName = "created_at"; break;
		default: orderColumnName = "id";
	}

	orderClause = " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC");

	// Build the main query
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ? OFFSET ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM accounts;";
	auto [_, totalCountResult] = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult.eof() ? 0 : totalCountResult.getIntField("count");

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM accounts WHERE name LIKE '%' || ? || '%';";
		auto [__, filteredCountResult] = ExecuteSelect(filteredCountQuery, search);
		filteredRecords = filteredCountResult.eof() ? 0 : filteredCountResult.getIntField("count");
	}

	// Execute main query
	auto [stmt, result] = !search.empty() ? 
		ExecuteSelect(mainQuery, search, length, start) :
		ExecuteSelect(mainQuery, length, start);

	// Build response JSON
	nlohmann::json accountsArray = nlohmann::json::array();

	while (!result.eof()) {
		nlohmann::json account = {
			{"id", result.getIntField("id")},
			{"name", result.getStringField("name")},
			{"banned", result.getIntField("banned")},
			{"locked", result.getIntField("locked")},
			{"gm_level", result.getIntField("gm_level")},
			{"mute_expire", result.getInt64Field("mute_expire")},
			{"created_at", result.getStringField("created_at")}
		};
		accountsArray.push_back(account);
		result.nextRow();
	}

	nlohmann::json response = {
		{"draw", 1},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", accountsArray}
	};

	return response;
}
nlohmann::json SQLiteDatabase::GetAccountById(uint32_t accountId) {
	try {
		auto [_, result] = ExecuteSelect("SELECT * FROM accounts WHERE id = ? LIMIT 1;", accountId);

		if (result.eof()) {
			return nlohmann::json{{"error", "Account not found"}};
		}

		nlohmann::json account = {
			{"id", result.getIntField("id")},
			{"name", result.getStringField("name")},
			{"banned", result.getIntField("banned")},
			{"locked", result.getIntField("locked")},
			{"gm_level", result.getIntField("gm_level")},
			{"mute_expire", result.getInt64Field("mute_expire")},
			{"created_at", result.getStringField("created_at")}
		};

		return account;
	} catch (const CppSQLite3Exception& e) {
		LOG_DEBUG("SQLite Error: %s", e.errorMessage());
		return nlohmann::json{{"error", "Database error"}};
	}
}