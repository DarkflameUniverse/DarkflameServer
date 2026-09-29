#include "SQLiteDatabase.h"

namespace {
	IPermissionGrants::Grant ReadGrant(CppSQLite3Query& result) {
		IPermissionGrants::Grant grant;
		grant.id = static_cast<uint64_t>(result.getInt64Field("id"));
		grant.targetType = result.getStringField("target_type");
		grant.targetId = result.getInt64Field("target_id");
		grant.kind = result.getStringField("kind");
		grant.name = result.getStringField("name");
		grant.deny = result.getIntField("deny") != 0;
		grant.expiresAt = result.getInt64Field("expires_at");
		grant.note = result.getStringField("note");
		grant.grantedAt = result.getInt64Field("granted_at");
		grant.grantedById = static_cast<uint32_t>(result.getInt64Field("granted_by_id"));
		grant.grantedBy = result.getStringField("granted_by");
		grant.revokedAt = result.getInt64Field("revoked_at");
		grant.revokedBy = result.getStringField("revoked_by");
		return grant;
	}
}

uint64_t SQLiteDatabase::InsertPermissionGrant(const Grant& grant) {
	ExecuteInsert("INSERT INTO permission_grants (target_type, target_id, kind, name, deny, expires_at, note, granted_at, granted_by_id, granted_by) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		grant.targetType, grant.targetId, grant.kind, grant.name, grant.deny, grant.expiresAt, grant.note, grant.grantedAt, grant.grantedById, grant.grantedBy);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("id"));
}

std::optional<IPermissionGrants::Grant> SQLiteDatabase::GetPermissionGrant(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM permission_grants WHERE id = ?;", static_cast<int64_t>(id));
	if (result.eof()) return std::nullopt;
	return ReadGrant(result);
}

std::vector<IPermissionGrants::Grant> SQLiteDatabase::GetPermissionGrants(const std::string& targetType, int64_t targetId) {
	std::vector<Grant> grants;
	auto [_, result] = ExecuteSelect("SELECT * FROM permission_grants WHERE target_type = ? AND target_id = ? ORDER BY id DESC;", targetType, targetId);
	for (; !result.eof(); result.nextRow()) grants.push_back(ReadGrant(result));
	return grants;
}

std::vector<IPermissionGrants::Grant> SQLiteDatabase::GetActivePermissionGrants(uint32_t accountId, int64_t characterId, int64_t now) {
	std::vector<Grant> grants;
	auto [_, result] = ExecuteSelect("SELECT * FROM permission_grants WHERE revoked_at = 0 AND (expires_at = 0 OR expires_at > ?) AND "
		"((target_type = 'account' AND target_id = ?) OR (? <> 0 AND target_type = 'character' AND target_id = ?)) ORDER BY id DESC;",
		now, static_cast<int64_t>(accountId), characterId, characterId);
	for (; !result.eof(); result.nextRow()) grants.push_back(ReadGrant(result));
	return grants;
}

std::vector<IPermissionGrants::Grant> SQLiteDatabase::GetRecentPermissionGrants(bool activeOnly, int64_t now, uint32_t limit) {
	std::vector<Grant> grants;
	auto [_, result] = activeOnly
		? ExecuteSelect("SELECT * FROM permission_grants WHERE revoked_at = 0 AND (expires_at = 0 OR expires_at > ?) ORDER BY id DESC LIMIT ?;", now, limit)
		: ExecuteSelect("SELECT * FROM permission_grants ORDER BY id DESC LIMIT ?;", limit);
	for (; !result.eof(); result.nextRow()) grants.push_back(ReadGrant(result));
	return grants;
}

bool SQLiteDatabase::RevokePermissionGrant(uint64_t id, const std::string& revokedBy, int64_t time) {
	return ExecuteUpdate("UPDATE permission_grants SET revoked_at = ?, revoked_by = ? WHERE id = ? AND revoked_at = 0;", time, revokedBy, static_cast<int64_t>(id)) > 0;
}
