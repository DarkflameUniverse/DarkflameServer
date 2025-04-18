#include "SQLiteDatabase.h"

std::vector<std::string> SQLiteDatabase::GetApprovedCharacterNames() {
	auto [_, result] = ExecuteSelect("SELECT name FROM charinfo;");

	std::vector<std::string> toReturn;

	while (!result.eof()) {
		toReturn.push_back(result.getStringField("name"));
		result.nextRow();
	}

	return toReturn;
}

std::optional<ICharInfo::Info> CharInfoFromQueryResult(CppSQLite3Query stmt) {
	if (stmt.eof()) {
		return std::nullopt;
	}

	ICharInfo::Info toReturn;

	toReturn.id = stmt.getIntField("id");
	toReturn.name = stmt.getStringField("name");
	toReturn.pendingName = stmt.getStringField("pending_name");
	toReturn.needsRename = stmt.getIntField("needs_rename");
	toReturn.cloneId = stmt.getInt64Field("prop_clone_id");
	toReturn.accountId = stmt.getIntField("account_id");
	toReturn.permissionMap = static_cast<ePermissionMap>(stmt.getIntField("permission_map"));

	return toReturn;
}

std::optional<ICharInfo::Info> SQLiteDatabase::GetCharacterInfo(const uint32_t charId) {
	return CharInfoFromQueryResult(
		ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE id = ? LIMIT 1;", charId).second
	);
}

std::optional<ICharInfo::Info> SQLiteDatabase::GetCharacterInfo(const std::string_view name) {
	return CharInfoFromQueryResult(
		ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE name = ? LIMIT 1;", name).second
	);
}

std::vector<uint32_t> SQLiteDatabase::GetAccountCharacterIds(const uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT id FROM charinfo WHERE account_id = ? ORDER BY last_login DESC LIMIT 4;", accountId);

	std::vector<uint32_t> toReturn;
	while (!result.eof()) {
		toReturn.push_back(result.getIntField("id"));
		result.nextRow();
	}

	return toReturn;
}

void SQLiteDatabase::InsertNewCharacter(const ICharInfo::Info info) {
	ExecuteInsert(
		"INSERT INTO `charinfo`(`id`, `account_id`, `name`, `pending_name`, `needs_rename`, `last_login`, `prop_clone_id`) VALUES (?,?,?,?,?,?,(SELECT IFNULL(MAX(`prop_clone_id`), 0) + 1 FROM `charinfo`))",
		info.id,
		info.accountId,
		info.name,
		info.pendingName,
		false,
		static_cast<uint32_t>(time(NULL)));
}

void SQLiteDatabase::SetCharacterName(const uint32_t characterId, const std::string_view name) {
	ExecuteUpdate("UPDATE charinfo SET name = ?, pending_name = '', needs_rename = 0, last_login = ? WHERE id = ?;", name, static_cast<uint32_t>(time(NULL)), characterId);
}

void SQLiteDatabase::SetPendingCharacterName(const uint32_t characterId, const std::string_view name) {
	ExecuteUpdate("UPDATE charinfo SET pending_name = ?, needs_rename = 0, last_login = ? WHERE id = ?;", name, static_cast<uint32_t>(time(NULL)), characterId);
}

void SQLiteDatabase::UpdateLastLoggedInCharacter(const uint32_t characterId) {
	ExecuteUpdate("UPDATE charinfo SET last_login = ? WHERE id = ?;", static_cast<uint32_t>(time(NULL)), characterId);
}

bool SQLiteDatabase::IsNameInUse(const std::string_view name) {
	auto [_, result] = ExecuteSelect("SELECT name FROM charinfo WHERE name = ? or pending_name = ? LIMIT 1;", name, name);

	return !result.eof();
}
