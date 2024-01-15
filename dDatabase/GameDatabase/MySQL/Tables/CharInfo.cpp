#include "MySQLDatabase.h"

std::vector<std::string> MySQLDatabase::GetApprovedCharacterNames() {
	auto result = ExecuteSelect("SELECT name FROM charinfo;");

	std::vector<std::string> toReturn;

	while (result->next()) {
		toReturn.push_back(result->getString("name").c_str());
	}

	return toReturn;
}

std::optional<ICharInfo::Info> CharInfoFromQueryResult(std::unique_ptr<sql::ResultSet> stmt) {
	if (!stmt->next()) {
		return std::nullopt;
	}

	ICharInfo::Info toReturn;

	toReturn.id = stmt->getUInt("id");
	toReturn.name = stmt->getString("name").c_str();
	toReturn.pendingName = stmt->getString("pending_name").c_str();
	toReturn.needsRename = stmt->getBoolean("needs_rename");
	toReturn.cloneId = stmt->getUInt64("prop_clone_id");
	toReturn.accountId = stmt->getUInt("account_id");
	toReturn.permissionMap = static_cast<ePermissionMap>(stmt->getUInt("permission_map"));

	return toReturn;
}

std::optional<ICharInfo::Info> MySQLDatabase::GetCharacterInfo(const uint32_t charId) {
	return CharInfoFromQueryResult(
		ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE id = ? LIMIT 1;", charId)
	);
}

std::optional<ICharInfo::Info> MySQLDatabase::GetCharacterInfo(const std::string_view name) {
	return CharInfoFromQueryResult(
		ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE name = ? LIMIT 1;", name)
	);
}

std::vector<uint32_t> MySQLDatabase::GetAccountCharacterIds(const uint32_t accountId) {
	auto result = ExecuteSelect("SELECT id FROM charinfo WHERE account_id = ? ORDER BY last_login DESC LIMIT 4;", accountId);

	std::vector<uint32_t> toReturn;
	toReturn.reserve(result->rowsCount());
	while (result->next()) {
		toReturn.push_back(result->getUInt("id"));
	}

	return toReturn;
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

void MySQLDatabase::SetCharacterName(const uint32_t characterId, const std::string_view name) {
	ExecuteUpdate("UPDATE charinfo SET name = ?, pending_name = '', needs_rename = 0, last_login = ? WHERE id = ? LIMIT 1;", name, static_cast<uint32_t>(time(NULL)), characterId);
}

void MySQLDatabase::SetPendingCharacterName(const uint32_t characterId, const std::string_view name) {
	ExecuteUpdate("UPDATE charinfo SET pending_name = ?, needs_rename = 0, last_login = ? WHERE id = ? LIMIT 1", name, static_cast<uint32_t>(time(NULL)), characterId);
}

void MySQLDatabase::UpdateLastLoggedInCharacter(const uint32_t characterId) {
	ExecuteUpdate("UPDATE charinfo SET last_login = ? WHERE id = ? LIMIT 1", static_cast<uint32_t>(time(NULL)), characterId);
}
