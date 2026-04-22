#include "MySQLDatabase.h"

#include "eGameMasterLevel.h"

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

void MySQLDatabase::UpdateAccountPlayKey(const uint32_t accountId, const uint32_t playKeyId) {
    ExecuteUpdate("UPDATE accounts SET play_key_id = ? WHERE id = ?;", playKeyId, accountId);
}

uint32_t MySQLDatabase::GetAccountCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM accounts;");
	return res->next() ? res->getUInt("count") : 0;
}

uint32_t MySQLDatabase::GetBannedAccountCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM accounts WHERE banned = 1;");
	return res->next() ? res->getUInt("count") : 0;
}

uint32_t MySQLDatabase::GetLockedAccountCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM accounts WHERE locked = 1;");
	return res->next() ? res->getUInt("count") : 0;
}

std::vector<IAccounts::ListInfo> MySQLDatabase::GetAllAccounts() {
	std::vector<IAccounts::ListInfo> out;
	auto res = ExecuteSelect("SELECT id, name, gm_level, banned, locked, mute_expire, play_key_id FROM accounts ORDER BY id ASC;");

	while (res->next()) {
		IAccounts::ListInfo info;
		info.id = res->getUInt("id");
		info.name = res->getString("name").c_str();
		info.gm_level = static_cast<eGameMasterLevel>(res->getInt("gm_level"));
		info.banned = res->getBoolean("banned");
		info.locked = res->getBoolean("locked");
		info.mute_expire = res->getUInt64("mute_expire");
		info.play_key_id = res->getUInt("play_key_id");
		out.push_back(info);
	}

	return out;
}

void MySQLDatabase::UpdateAccountLock(const uint32_t accountId, const bool locked) {
	ExecuteUpdate("UPDATE accounts SET locked = ? WHERE id = ?;", locked, accountId);
}

std::optional<IAccounts::DetailedInfo> MySQLDatabase::GetAccountById(const uint32_t accountId) {
	auto result = ExecuteSelect(
		"SELECT id, name, email, gm_level, banned, locked, mute_expire, play_key_id, created_at FROM accounts WHERE id = ? LIMIT 1;",
		accountId
	);

	if (!result->next()) {
		return std::nullopt;
	}

	IAccounts::DetailedInfo info;
	info.id = result->getUInt("id");
	info.name = result->getString("name").c_str();
	info.email = result->getString("email").c_str();
	info.gm_level = static_cast<eGameMasterLevel>(result->getInt("gm_level"));
	info.banned = result->getBoolean("banned");
	info.locked = result->getBoolean("locked");
	info.mute_expire = result->getUInt64("mute_expire");
	info.play_key_id = result->getUInt("play_key_id");
	info.created_at = result->getUInt64("created_at");

	return info;
}

void MySQLDatabase::UpdateAccountEmail(const uint32_t accountId, const std::string_view email) {
	ExecuteUpdate("UPDATE accounts SET email = ? WHERE id = ?;", email, accountId);
}

void MySQLDatabase::DeleteAccount(const uint32_t accountId) {
	// Delete all associated data first
	// Characters and their data will be handled by CASCADE or manual deletion
	ExecuteDelete("DELETE FROM char_info WHERE account_id = ?;", accountId);
	ExecuteDelete("DELETE FROM accounts WHERE id = ?;", accountId);
}

std::vector<IAccounts::SessionInfo> MySQLDatabase::GetAccountSessions(const uint32_t accountId, uint32_t limit) {
	// account_sessions table doesn't exist in the current schema
	// Session tracking would need to be implemented separately
	return {};
}
