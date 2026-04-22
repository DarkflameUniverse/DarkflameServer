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

	toReturn.id = stmt.getInt64Field("id");
	toReturn.name = stmt.getStringField("name");
	toReturn.pendingName = stmt.getStringField("pending_name");
	toReturn.needsRename = stmt.getIntField("needs_rename");
	toReturn.cloneId = stmt.getInt64Field("prop_clone_id");
	toReturn.accountId = stmt.getIntField("account_id");
	toReturn.permissionMap = static_cast<ePermissionMap>(stmt.getIntField("permission_map"));

	return toReturn;
}

std::optional<ICharInfo::Info> SQLiteDatabase::GetCharacterInfo(const LWOOBJID charId) {
	return CharInfoFromQueryResult(
		ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE id = ? LIMIT 1;", charId).second
	);
}

std::optional<ICharInfo::Info> SQLiteDatabase::GetCharacterInfo(const std::string_view name) {
	return CharInfoFromQueryResult(
		ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE name = ? LIMIT 1;", name).second
	);
}

std::vector<LWOOBJID> SQLiteDatabase::GetAccountCharacterIds(const LWOOBJID accountId) {
	auto [_, result] = ExecuteSelect("SELECT id FROM charinfo WHERE account_id = ? ORDER BY last_login DESC LIMIT 4;", accountId);

	std::vector<LWOOBJID> toReturn;
	while (!result.eof()) {
		toReturn.push_back(result.getInt64Field("id"));
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

void SQLiteDatabase::SetCharacterName(const LWOOBJID characterId, const std::string_view name) {
	ExecuteUpdate("UPDATE charinfo SET name = ?, pending_name = '', needs_rename = 0, last_login = ? WHERE id = ?;", name, static_cast<uint32_t>(time(NULL)), characterId);
}

void SQLiteDatabase::SetPendingCharacterName(const LWOOBJID characterId, const std::string_view name) {
	ExecuteUpdate("UPDATE charinfo SET pending_name = ?, needs_rename = 0, last_login = ? WHERE id = ?;", name, static_cast<uint32_t>(time(NULL)), characterId);
}

void SQLiteDatabase::UpdateLastLoggedInCharacter(const LWOOBJID characterId) {
	ExecuteUpdate("UPDATE charinfo SET last_login = ? WHERE id = ?;", static_cast<uint32_t>(time(NULL)), characterId);
}

bool SQLiteDatabase::IsNameInUse(const std::string_view name) {
	auto [_, result] = ExecuteSelect("SELECT name FROM charinfo WHERE name = ? or pending_name = ? LIMIT 1;", name, name);

	return !result.eof();
}

uint32_t SQLiteDatabase::GetCharacterCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM charinfo;");
	if (res.eof()) return 0;
	return res.getIntField("count");
}

std::vector<ICharInfo::Info> SQLiteDatabase::GetAllCharactersPaginated(
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

	auto [stmt, res] = ExecuteSelect(query, limit, offset);

	while (!res.eof()) {
		ICharInfo::Info info;
		info.id = res.getInt64Field("id");
		info.name = res.getStringField("name");
		info.pendingName = res.getStringField("pending_name");
		info.needsRename = res.getIntField("needs_rename");
		info.cloneId = res.getInt64Field("prop_clone_id");
		info.accountId = res.getIntField("account_id");
		info.permissionMap = static_cast<ePermissionMap>(res.getIntField("permission_map"));
		out.push_back(info);
		res.nextRow();
	}

	return out;
}

std::vector<ICharInfo::Info> SQLiteDatabase::GetCharactersWithPendingNames() {
	std::vector<ICharInfo::Info> out;

	auto [stmt, res] = ExecuteSelect("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map, id, account_id FROM charinfo WHERE pending_name != '' ORDER BY id ASC;");

	while (!res.eof()) {
		ICharInfo::Info info;
		info.id = res.getInt64Field("id");
		info.name = res.getStringField("name");
		info.pendingName = res.getStringField("pending_name");
		info.needsRename = res.getIntField("needs_rename");
		info.cloneId = res.getInt64Field("prop_clone_id");
		info.accountId = res.getIntField("account_id");
		info.permissionMap = static_cast<ePermissionMap>(res.getIntField("permission_map"));
		out.push_back(info);
		res.nextRow();
	}

	return out;
}

// SQLite-specific implementations added: UpdateCharacterPermissions and SetCharacterNeedsRename
void SQLiteDatabase::UpdateCharacterPermissions(const LWOOBJID characterId, const ePermissionMap permissions) {
	ExecuteUpdate("UPDATE charinfo SET permission_map = ? WHERE id = ?;", static_cast<uint64_t>(permissions), characterId);
}

void SQLiteDatabase::SetCharacterNeedsRename(const LWOOBJID characterId, const bool needsRename) {
	ExecuteUpdate("UPDATE charinfo SET needs_rename = ? WHERE id = ?;", needsRename ? 1 : 0, characterId);
}

std::optional<ICharInfo::Stats> SQLiteDatabase::GetCharacterStats(const LWOOBJID characterId) {
	// char_stats table doesn't exist in the current schema
	// Stats would need to be parsed from charxml or a new table created
	return std::nullopt;
}

std::vector<ICharInfo::InventoryItem> SQLiteDatabase::GetCharacterInventory(const LWOOBJID characterId) {
	// Inventory data is stored in charxml, not a separate table
	// Would need to parse the XML to extract inventory items
	// Returning empty for now - implement XML parsing if needed
	return {};
}

std::vector<ICharInfo::Activity> SQLiteDatabase::GetCharacterActivity(const LWOOBJID characterId, uint32_t limit) {
	auto [_, res] = ExecuteSelect(
		"SELECT time, activity, map_id FROM activity_log WHERE character_id = ? ORDER BY time DESC LIMIT ?;",
		characterId, limit
	);

	std::vector<ICharInfo::Activity> activities;
	while (!res.eof()) {
		ICharInfo::Activity activity;
		activity.timestamp = res.getInt64Field("time");
		activity.activity = static_cast<eActivityType>(res.getIntField("activity"));
		activity.mapId = res.getIntField("map_id");
		activities.push_back(activity);
		res.nextRow();
	}

	return activities;
}

void SQLiteDatabase::RescueCharacter(const LWOOBJID characterId, uint32_t zoneId) {
	// The rescue is now handled by the chat server API which kicks the player
	// and modifies the XML after the player data is saved
	// This database method is intentionally a no-op as the actual work
	// is done via DashboardHelpers::RescueCharacter()
}
