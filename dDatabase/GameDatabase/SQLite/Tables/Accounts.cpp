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

void SQLiteDatabase::UpdateAccountPlayKey(const uint32_t accountId, const uint32_t playKeyId) {
    ExecuteUpdate("UPDATE accounts SET play_key_id = ? WHERE id = ?;", playKeyId, accountId);
}

uint32_t SQLiteDatabase::GetAccountCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM accounts;");
	if (res.eof()) return 0;

	return res.getIntField("count");
}

uint32_t SQLiteDatabase::GetBannedAccountCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM accounts WHERE banned = 1;");
	if (res.eof()) return 0;

	return res.getIntField("count");
}

uint32_t SQLiteDatabase::GetLockedAccountCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM accounts WHERE locked = 1;");
	if (res.eof()) return 0;

	return res.getIntField("count");
}

std::vector<IAccounts::ListInfo> SQLiteDatabase::GetAllAccounts() {
	std::vector<IAccounts::ListInfo> out;
	auto [stmt, res] = ExecuteSelect("SELECT id, name, gm_level, banned, locked, mute_expire, play_key_id FROM accounts ORDER BY id ASC;");

	while (!res.eof()) {
		IAccounts::ListInfo info;
		info.id = res.getIntField("id");
		info.name = res.getStringField("name");
		info.gm_level = static_cast<eGameMasterLevel>(res.getIntField("gm_level"));
		info.banned = res.getIntField("banned");
		info.locked = res.getIntField("locked");
		info.mute_expire = static_cast<uint64_t>(res.getInt64Field("mute_expire"));
		info.play_key_id = res.getIntField("play_key_id");
		out.push_back(info);
		res.nextRow();
	}

	return out;
}

void SQLiteDatabase::UpdateAccountLock(const uint32_t accountId, const bool locked) {
	ExecuteUpdate("UPDATE accounts SET locked = ? WHERE id = ?;", locked, accountId);
}

std::optional<IAccounts::DetailedInfo> SQLiteDatabase::GetAccountById(const uint32_t accountId) {
	auto [_, result] = ExecuteSelect(
		"SELECT id, name, email, gm_level, banned, locked, mute_expire, play_key_id, created_at FROM accounts WHERE id = ? LIMIT 1;",
		accountId
	);

	if (result.eof()) {
		return std::nullopt;
	}

	IAccounts::DetailedInfo info;
	info.id = result.getIntField("id");
	info.name = result.getStringField("name");
	info.email = result.getStringField("email");
	info.gm_level = static_cast<eGameMasterLevel>(result.getIntField("gm_level"));
	info.banned = result.getIntField("banned") != 0;
	info.locked = result.getIntField("locked") != 0;
	info.mute_expire = static_cast<uint64_t>(result.getInt64Field("mute_expire"));
	info.play_key_id = result.getIntField("play_key_id");
	info.created_at = static_cast<uint64_t>(result.getInt64Field("created_at"));

	return info;
}

void SQLiteDatabase::UpdateAccountEmail(const uint32_t accountId, const std::string_view email) {
	ExecuteUpdate("UPDATE accounts SET email = ? WHERE id = ?;", email, accountId);
}

void SQLiteDatabase::DeleteAccount(const uint32_t accountId) {
	// Delete all associated data first
	ExecuteDelete("DELETE FROM char_info WHERE account_id = ?;", accountId);
	ExecuteDelete("DELETE FROM accounts WHERE id = ?;", accountId);
}

std::vector<IAccounts::SessionInfo> SQLiteDatabase::GetAccountSessions(const uint32_t accountId, uint32_t limit) {
	// account_sessions table doesn't exist in the current schema
	// Session tracking would need to be implemented separately
	return {};
}
