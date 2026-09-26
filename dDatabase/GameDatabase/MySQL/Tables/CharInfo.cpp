#include "MySQLDatabase.h"
#include "json.hpp"

std::vector<std::string> MySQLDatabase::GetApprovedCharacterNames() {
	auto result = ExecuteSelect("SELECT name FROM charinfo;");

	std::vector<std::string> toReturn;

	while (result->next()) {
		toReturn.push_back(result->getString("name").c_str());
	}

	return toReturn;
}

std::optional<ICharInfo::Info> CharInfoFromQueryResult(PreparedStmtResultSet& stmt) {
	if (!stmt->next()) {
		return std::nullopt;
	}

	ICharInfo::Info toReturn;

	toReturn.id = stmt->getInt64("id");
	toReturn.name = stmt->getString("name").c_str();
	toReturn.pendingName = stmt->getString("pending_name").c_str();
	toReturn.needsRename = stmt->getBoolean("needs_rename");
	toReturn.cloneId = stmt->getUInt64("prop_clone_id");
	toReturn.accountId = stmt->getUInt("account_id");
	toReturn.permissionMap = static_cast<ePermissionMap>(stmt->getUInt("permission_map"));

	return toReturn;
}

std::optional<ICharInfo::Info> MySQLDatabase::GetCharacterInfo(const LWOOBJID charId) {
	auto result = ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE id = ? LIMIT 1;", charId);
	return CharInfoFromQueryResult(result);
}

std::optional<ICharInfo::Info> MySQLDatabase::GetCharacterInfo(const std::string_view name) {
	auto result = ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE name = ? LIMIT 1;", name);
	return CharInfoFromQueryResult(result);
}

std::vector<LWOOBJID> MySQLDatabase::GetAccountCharacterIds(const LWOOBJID accountId) {
	auto result = ExecuteSelect("SELECT id FROM charinfo WHERE account_id = ? ORDER BY last_login DESC LIMIT 4;", accountId);

	std::vector<LWOOBJID> toReturn;
	toReturn.reserve(result->rowsCount());
	while (result->next()) {
		toReturn.push_back(result->getInt64("id"));
	}

	return toReturn;
}

uint32_t MySQLDatabase::GetCharacterCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM charinfo;");
	return res->next() ? res->getUInt("count") : 0;
}

void MySQLDatabase::InsertNewCharacter(const ICharInfo::Info info) {
	ExecuteInsert(
		"INSERT INTO `charinfo`(`id`, `account_id`, `name`, `pending_name`, `needs_rename`, `last_login`) VALUES (?,?,?,?,?,?)",
		info.id,
		info.accountId,
		info.name,
		info.pendingName,
		false,
		static_cast<uint32_t>(time(NULL)));
}

void MySQLDatabase::SetCharacterName(const LWOOBJID characterId, const std::string_view name) {
	ExecuteUpdate("UPDATE charinfo SET name = ?, pending_name = '', needs_rename = 0, last_login = ? WHERE id = ? LIMIT 1;", name, static_cast<uint32_t>(time(NULL)), characterId);
}

void MySQLDatabase::SetPendingCharacterName(const LWOOBJID characterId, const std::string_view name) {
	ExecuteUpdate("UPDATE charinfo SET pending_name = ?, needs_rename = 0, last_login = ? WHERE id = ? LIMIT 1", name, static_cast<uint32_t>(time(NULL)), characterId);
}

void MySQLDatabase::UpdateLastLoggedInCharacter(const LWOOBJID characterId) {
	ExecuteUpdate("UPDATE charinfo SET last_login = ? WHERE id = ? LIMIT 1", static_cast<uint32_t>(time(NULL)), characterId);
}

bool MySQLDatabase::IsNameInUse(const std::string_view name) {
	auto result = ExecuteSelect("SELECT name FROM charinfo WHERE name = ? or pending_name = ? LIMIT 1;", name, name);

	return result->next();
}

nlohmann::json MySQLDatabase::GetCharacterById(const LWOOBJID charId) {
	auto result = ExecuteSelect(
		"SELECT c.id, c.name, c.pending_name, c.needs_rename, c.account_id, c.last_login, c.prop_clone_id, c.permission_map, a.name as account_name "
		"FROM charinfo c LEFT JOIN accounts a ON c.account_id = a.id WHERE c.id = ? LIMIT 1;", charId);
	if (!result->next()) {
		return nlohmann::json({{"error", "Character not found"}});
	}
	return nlohmann::json({
		{"id", std::to_string(result->getInt64("id"))},
		{"name", result->getString("name")},
		{"pending_name", result->getString("pending_name")},
		{"needs_rename", result->getBoolean("needs_rename")},
		{"account_id", result->getUInt("account_id")},
		{"account_name", result->getString("account_name")},
		{"last_login", result->getUInt64("last_login")},
		{"prop_clone_id", result->getUInt64("prop_clone_id")},
		{"permission_map", result->getUInt("permission_map")}
	});
}
