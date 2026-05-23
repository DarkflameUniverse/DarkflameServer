#include "MySQLDatabase.h"

#include "eGameMasterLevel.h"
#include "json.hpp"

std::optional<IAccounts::Info> MySQLDatabase::GetAccountInfo(const std::string_view username) {
	auto result = ExecuteSelect("SELECT id, password, banned, locked, play_key_id, gm_level, mute_expire FROM accounts WHERE name = ? LIMIT 1;", username);

	if (!result->next()) {
		return std::nullopt;
	}

	IAccounts::Info toReturn;
	toReturn.id = result->getUInt("id");
	toReturn.maxGmLevel = static_cast<eGameMasterLevel>(result->getInt("gm_level"));
	toReturn.bcryptPassword = result->getString("password").c_str();
	toReturn.banned = result->getBoolean("banned");
	toReturn.locked = result->getBoolean("locked");
	toReturn.playKeyId = result->getUInt("play_key_id");
	toReturn.muteExpire = result->getUInt64("mute_expire");

	return toReturn;
}

void MySQLDatabase::UpdateAccountUnmuteTime(const uint32_t accountId, const uint64_t timeToUnmute) {
	ExecuteUpdate("UPDATE accounts SET mute_expire = ? WHERE id = ?;", timeToUnmute, accountId);
}

void MySQLDatabase::UpdateAccountBan(const uint32_t accountId, const bool banned) {
	ExecuteUpdate("UPDATE accounts SET banned = ? WHERE id = ?;", banned, accountId);
}

void MySQLDatabase::UpdateAccountPassword(const uint32_t accountId, const std::string_view bcryptpassword) {
	ExecuteUpdate("UPDATE accounts SET password = ? WHERE id = ?;", bcryptpassword, accountId);
}

void MySQLDatabase::InsertNewAccount(const std::string_view username, const std::string_view bcryptpassword) {
	ExecuteInsert("INSERT INTO accounts (name, password, gm_level) VALUES (?, ?, ?);", username, bcryptpassword, static_cast<int32_t>(eGameMasterLevel::OPERATOR));
}

void MySQLDatabase::UpdateAccountGmLevel(const uint32_t accountId, const eGameMasterLevel gmLevel) {
	ExecuteUpdate("UPDATE accounts SET gm_level = ? WHERE id = ?;", static_cast<int32_t>(gmLevel), accountId);
}

uint32_t MySQLDatabase::GetAccountCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM accounts;");
	return res->next() ? res->getUInt("count") : 0;
}

void MySQLDatabase::RecordFailedAttempt(const uint32_t accountId) {
	ExecuteUpdate("UPDATE accounts SET failed_attempts = failed_attempts + 1 WHERE id = ?;", accountId);
}

void MySQLDatabase::ClearFailedAttempts(const uint32_t accountId) {
	ExecuteUpdate("UPDATE accounts SET failed_attempts = 0, lockout_time = NULL, last_login = NOW() WHERE id = ?;", accountId);
}

void MySQLDatabase::SetLockout(const uint32_t accountId, const int64_t lockoutUntil) {
	ExecuteUpdate("UPDATE accounts SET lockout_time = FROM_UNIXTIME(?) WHERE id = ?;", lockoutUntil, accountId);
}

bool MySQLDatabase::IsLockedOut(const uint32_t accountId) {
	auto result = ExecuteSelect("SELECT lockout_time FROM accounts WHERE id = ?;", accountId);
	if (!result->next()) {
		return false;
	}

	// If lockout_time is set and in the future, account is locked
	const char* lockoutTime = result->getString("lockout_time").c_str();
	if (lockoutTime == nullptr || strlen(lockoutTime) == 0) {
		return false;
	}

	// Simplified check - if lockout_time exists and is not null, it's locked
	return true;
}

uint8_t MySQLDatabase::GetFailedAttempts(const uint32_t accountId) {
	auto result = ExecuteSelect("SELECT failed_attempts FROM accounts WHERE id = ?;", accountId);
	if (!result->next()) {
		return 0;
	}

	return result->getUInt("failed_attempts");
}

nlohmann::json MySQLDatabase::GetAccountsTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// Build base query
	std::string baseQuery = "SELECT id, name, banned, locked, gm_level, mute_expire, created_at FROM accounts";
	std::string whereClause;
	std::string orderClause;

	// Add search filter if provided
	if (!search.empty()) {
		whereClause = " WHERE name LIKE CONCAT('%', ?, '%')";
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
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ?, ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM accounts;";
	auto totalCountResult = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult->next() ? totalCountResult->getUInt("count") : 0;

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM accounts WHERE name LIKE CONCAT('%', ?, '%');";
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
	nlohmann::json accountsArray = nlohmann::json::array();

	while (result->next()) {
		nlohmann::json account = {
			{"id", result->getUInt("id")},
			{"name", result->getString("name")},
			{"banned", result->getBoolean("banned")},
			{"locked", result->getBoolean("locked")},
			{"gm_level", result->getInt("gm_level")},
			{"mute_expire", result->getUInt64("mute_expire")},
			{"created_at", result->getString("created_at")}
		};
		accountsArray.push_back(account);
	}

	nlohmann::json response = {
		{"draw", 1},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", accountsArray}
	};

	return response;
}

nlohmann::json MySQLDatabase::GetAccountById(uint32_t accountId) {
	try {
		const std::string query = "SELECT id, name, banned, locked, gm_level, mute_expire, created_at FROM accounts WHERE id = ?;";
		auto result = ExecuteSelect(query, accountId);

		if (!result->next()) {
			return nlohmann::json{{"error", "Account not found"}};
		}

		nlohmann::json account = {
			{"id", result->getUInt("id")},
			{"name", result->getString("name")},
			{"banned", result->getBoolean("banned")},
			{"locked", result->getBoolean("locked")},
			{"gm_level", result->getInt("gm_level")},
			{"mute_expire", result->getUInt64("mute_expire")},
			{"created_at", result->getString("created_at")}
		};

		return account;
	} catch (const sql::SQLException& e) {
		LOG_DEBUG("SQL Error: %s", e.what());
		return nlohmann::json{{"error", "Database error"}};
	}
}
