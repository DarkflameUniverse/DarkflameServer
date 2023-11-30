#include "MySQLDatabase.h"

#include "eGameMasterLevel.h"

std::optional<IAccounts::Info> MySQLDatabase::GetAccountInfo(const std::string_view username) {
	auto result = ExecuteSelect("SELECT id, password, banned, locked, play_key_id, gm_level FROM accounts WHERE name = ? LIMIT 1;", username);

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
