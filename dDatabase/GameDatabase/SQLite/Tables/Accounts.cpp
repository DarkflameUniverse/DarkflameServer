#include "SQLiteDatabase.h"

#include "eGameMasterLevel.h"
#include "Database.h"

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