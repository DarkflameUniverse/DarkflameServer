#include "SQLiteDatabase.h"

std::optional<IZoneEditing::ZoneLock> SQLiteDatabase::GetZoneLock(uint32_t zoneId) {
	auto [_, result] = ExecuteSelect("SELECT zone_id, account_id, locked_at FROM zone_edit_locks WHERE zone_id = ?;", zoneId);
	if (result.eof()) return std::nullopt;
	return ZoneLock{ static_cast<uint32_t>(result.getIntField("zone_id")), static_cast<uint32_t>(result.getIntField("account_id")), result.getInt64Field("locked_at") };
}

std::vector<IZoneEditing::ZoneLock> SQLiteDatabase::GetZoneLocks() {
	std::vector<ZoneLock> locks;
	auto [_, result] = ExecuteSelect("SELECT zone_id, account_id, locked_at FROM zone_edit_locks ORDER BY zone_id;");
	for (; !result.eof(); result.nextRow()) {
		locks.push_back({ static_cast<uint32_t>(result.getIntField("zone_id")), static_cast<uint32_t>(result.getIntField("account_id")), result.getInt64Field("locked_at") });
	}
	return locks;
}

bool SQLiteDatabase::TakeZoneLock(uint32_t zoneId, uint32_t accountId, int64_t time) {
	return ExecuteUpdate("INSERT OR IGNORE INTO zone_edit_locks (zone_id, account_id, locked_at) VALUES (?, ?, ?);", zoneId, accountId, time) > 0;
}

bool SQLiteDatabase::ReleaseZoneLock(uint32_t zoneId) {
	return ExecuteUpdate("DELETE FROM zone_edit_locks WHERE zone_id = ?;", zoneId) > 0;
}

std::vector<IZoneEditing::ZoneEditor> SQLiteDatabase::GetZoneEditors(uint32_t zoneId) {
	std::vector<ZoneEditor> editors;
	auto [_, result] = ExecuteSelect(
		"SELECT zone_id, account_id, assigned_by, assigned_at FROM zone_editors WHERE (? = 0 OR zone_id = ?) ORDER BY zone_id, account_id;", zoneId, zoneId);
	for (; !result.eof(); result.nextRow()) {
		editors.push_back({ static_cast<uint32_t>(result.getIntField("zone_id")), static_cast<uint32_t>(result.getIntField("account_id")),
			result.getStringField("assigned_by"), result.getInt64Field("assigned_at") });
	}
	return editors;
}

bool SQLiteDatabase::IsZoneEditor(uint32_t zoneId, uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT 1 FROM zone_editors WHERE zone_id = ? AND account_id = ?;", zoneId, accountId);
	return !result.eof();
}

void SQLiteDatabase::AddZoneEditor(const ZoneEditor& editor) {
	ExecuteInsert("INSERT OR IGNORE INTO zone_editors (zone_id, account_id, assigned_by, assigned_at) VALUES (?, ?, ?, ?);",
		editor.zoneId, editor.accountId, editor.assignedBy, editor.assignedAt);
}

bool SQLiteDatabase::RemoveZoneEditor(uint32_t zoneId, uint32_t accountId) {
	return ExecuteUpdate("DELETE FROM zone_editors WHERE zone_id = ? AND account_id = ?;", zoneId, accountId) > 0;
}
