#include "MySQLDatabase.h"

std::optional<IZoneEditing::ZoneLock> MySQLDatabase::GetZoneLock(uint32_t zoneId) {
	auto result = ExecuteSelect("SELECT zone_id, account_id, locked_at FROM zone_edit_locks WHERE zone_id = ?;", zoneId);
	if (!result->next()) return std::nullopt;
	return ZoneLock{ result->getUInt("zone_id"), result->getUInt("account_id"), result->getInt64("locked_at") };
}

std::vector<IZoneEditing::ZoneLock> MySQLDatabase::GetZoneLocks() {
	std::vector<ZoneLock> locks;
	auto result = ExecuteSelect("SELECT zone_id, account_id, locked_at FROM zone_edit_locks ORDER BY zone_id;");
	while (result->next()) locks.push_back({ result->getUInt("zone_id"), result->getUInt("account_id"), result->getInt64("locked_at") });
	return locks;
}

bool MySQLDatabase::TakeZoneLock(uint32_t zoneId, uint32_t accountId, int64_t time) {
	return ExecuteUpdate("INSERT IGNORE INTO zone_edit_locks (zone_id, account_id, locked_at) VALUES (?, ?, ?);", zoneId, accountId, time) > 0;
}

bool MySQLDatabase::ReleaseZoneLock(uint32_t zoneId) {
	return ExecuteUpdate("DELETE FROM zone_edit_locks WHERE zone_id = ?;", zoneId) > 0;
}

std::vector<IZoneEditing::ZoneEditor> MySQLDatabase::GetZoneEditors(uint32_t zoneId) {
	std::vector<ZoneEditor> editors;
	auto result = ExecuteSelect(
		"SELECT zone_id, account_id, assigned_by, assigned_at FROM zone_editors WHERE (? = 0 OR zone_id = ?) ORDER BY zone_id, account_id;", zoneId, zoneId);
	while (result->next()) {
		editors.push_back({ result->getUInt("zone_id"), result->getUInt("account_id"), result->getString("assigned_by").c_str(), result->getInt64("assigned_at") });
	}
	return editors;
}

bool MySQLDatabase::IsZoneEditor(uint32_t zoneId, uint32_t accountId) {
	auto result = ExecuteSelect("SELECT 1 FROM zone_editors WHERE zone_id = ? AND account_id = ?;", zoneId, accountId);
	return result->next();
}

void MySQLDatabase::AddZoneEditor(const ZoneEditor& editor) {
	ExecuteInsert("INSERT IGNORE INTO zone_editors (zone_id, account_id, assigned_by, assigned_at) VALUES (?, ?, ?, ?);",
		editor.zoneId, editor.accountId, editor.assignedBy, editor.assignedAt);
}

bool MySQLDatabase::RemoveZoneEditor(uint32_t zoneId, uint32_t accountId) {
	return ExecuteUpdate("DELETE FROM zone_editors WHERE zone_id = ? AND account_id = ?;", zoneId, accountId) > 0;
}
