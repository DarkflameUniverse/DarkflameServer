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
	return CharInfoFromQueryResult(
		ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE id = ? LIMIT 1;", charId)
	);
}

std::optional<ICharInfo::Info> MySQLDatabase::GetCharacterInfo(const std::string_view name) {
	return CharInfoFromQueryResult(
		ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE name = ? LIMIT 1;", name)
	);
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

uint32_t MySQLDatabase::GetCharacterCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM charinfo;");
	return res->next() ? res->getUInt("count") : 0;
}

std::vector<ICharInfo::Info> MySQLDatabase::GetAllCharactersPaginated(
	uint32_t offset,
	uint32_t limit,
	const std::string& orderColumn,
	const std::string& orderDir
) {
	std::vector<ICharInfo::Info> out;

	// Validate orderColumn to prevent SQL injection
	std::string validColumn = "id";
	if (orderColumn == "name" || orderColumn == "account_id" || orderColumn == "id" || orderColumn == "last_login") {
		validColumn = orderColumn;
	}

	// Validate orderDir
	std::string validDir = (orderDir == "ASC" || orderDir == "asc") ? "ASC" : "DESC";

	// Build query
	std::string query = "SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo ORDER BY " +
		validColumn + " " + validDir + " LIMIT ? OFFSET ?;";

	auto res = ExecuteSelect(query, limit, offset);

	while (res->next()) {
		ICharInfo::Info info;
		info.id = res->getInt64("id");
		info.name = res->getString("name").c_str();
		info.pendingName = res->getString("pending_name").c_str();
		info.needsRename = res->getBoolean("needs_rename");
		info.cloneId = res->getUInt64("prop_clone_id");
		info.accountId = res->getUInt("account_id");
		info.permissionMap = static_cast<ePermissionMap>(res->getUInt("permission_map"));
		out.push_back(info);
	}

	return out;
}

std::vector<ICharInfo::Info> MySQLDatabase::GetCharactersWithPendingNames() {
	std::vector<ICharInfo::Info> out;

	auto res = ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE pending_name != '' ORDER BY id ASC;");

	while (res->next()) {
		ICharInfo::Info info;
		info.id = res->getInt64("id");
		info.name = res->getString("name").c_str();
		info.pendingName = res->getString("pending_name").c_str();
		info.needsRename = res->getBoolean("needs_rename");
		info.cloneId = res->getUInt64("prop_clone_id");
		info.accountId = res->getUInt("account_id");
		info.permissionMap = static_cast<ePermissionMap>(res->getUInt("permission_map"));
		out.push_back(info);
	}

	return out;
}

void MySQLDatabase::UpdateCharacterPermissions(const LWOOBJID characterId, ePermissionMap permissions) {
	ExecuteUpdate("UPDATE charinfo SET permission_map = ? WHERE id = ? LIMIT 1;", static_cast<uint64_t>(permissions), characterId);
}

void MySQLDatabase::SetCharacterNeedsRename(const LWOOBJID characterId, bool needsRename) {
	ExecuteUpdate("UPDATE charinfo SET needs_rename = ? WHERE id = ? LIMIT 1;", needsRename, characterId);
}

std::optional<ICharInfo::Stats> MySQLDatabase::GetCharacterStats(const LWOOBJID characterId) {
	// char_stats table doesn't exist in the current schema
	// Stats would need to be parsed from charxml or a new table created
	return std::nullopt;
}

std::vector<ICharInfo::InventoryItem> MySQLDatabase::GetCharacterInventory(const LWOOBJID characterId) {
	// Inventory data is stored in charxml, not a separate table
	// Would need to parse the XML to extract inventory items
	// Returning empty for now - implement XML parsing if needed
	return {};
}

std::vector<ICharInfo::Activity> MySQLDatabase::GetCharacterActivity(const LWOOBJID characterId, uint32_t limit) {
	auto res = ExecuteSelect(
		"SELECT time, activity, map_id FROM activity_log WHERE character_id = ? ORDER BY time DESC LIMIT ?;",
		characterId, limit
	);

	std::vector<ICharInfo::Activity> activities;
	while (res->next()) {
		ICharInfo::Activity activity;
		activity.timestamp = res->getUInt64("time");
		activity.activity = static_cast<eActivityType>(res->getUInt("activity"));
		activity.mapId = res->getUInt("map_id");
		activities.push_back(activity);
	}

	return activities;
}

void MySQLDatabase::RescueCharacter(const LWOOBJID characterId, uint32_t zoneId) {
	// The rescue is now handled by the chat server API which kicks the player
	// and modifies the XML after the player data is saved
	// This database method is intentionally a no-op as the actual work
	// is done via DashboardHelpers::RescueCharacter()
}
