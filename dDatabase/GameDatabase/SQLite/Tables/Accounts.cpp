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
