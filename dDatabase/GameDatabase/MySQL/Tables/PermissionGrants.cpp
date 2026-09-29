#include "MySQLDatabase.h"

namespace {
	template<typename Result> IPermissionGrants::Grant ReadGrant(Result& result) {
		IPermissionGrants::Grant grant;
		grant.id = result->getUInt64("id");
		grant.targetType = result->getString("target_type").c_str();
		grant.targetId = result->getInt64("target_id");
		grant.kind = result->getString("kind").c_str();
		grant.name = result->getString("name").c_str();
		grant.deny = result->getInt("deny") != 0;
		grant.expiresAt = result->getInt64("expires_at");
		grant.note = result->getString("note").c_str();
		grant.grantedAt = result->getInt64("granted_at");
		grant.grantedById = result->getUInt("granted_by_id");
		grant.grantedBy = result->getString("granted_by").c_str();
		grant.revokedAt = result->getInt64("revoked_at");
		grant.revokedBy = result->getString("revoked_by").c_str();
		return grant;
	}
}

uint64_t MySQLDatabase::InsertPermissionGrant(const Grant& grant) {
	ExecuteInsert("INSERT INTO permission_grants (target_type, target_id, kind, name, deny, expires_at, note, granted_at, granted_by_id, granted_by) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		grant.targetType, grant.targetId, grant.kind, grant.name, grant.deny, grant.expiresAt, grant.note, grant.grantedAt, grant.grantedById, grant.grantedBy);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getUInt64("id") : 0;
}

std::optional<IPermissionGrants::Grant> MySQLDatabase::GetPermissionGrant(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM permission_grants WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return ReadGrant(result);
}

std::vector<IPermissionGrants::Grant> MySQLDatabase::GetPermissionGrants(const std::string& targetType, int64_t targetId) {
	std::vector<Grant> grants;
	auto result = ExecuteSelect("SELECT * FROM permission_grants WHERE target_type = ? AND target_id = ? ORDER BY id DESC;", targetType, targetId);
	while (result->next()) grants.push_back(ReadGrant(result));
	return grants;
}

std::vector<IPermissionGrants::Grant> MySQLDatabase::GetActivePermissionGrants(uint32_t accountId, int64_t characterId, int64_t now) {
	std::vector<Grant> grants;
	auto result = ExecuteSelect("SELECT * FROM permission_grants WHERE revoked_at = 0 AND (expires_at = 0 OR expires_at > ?) AND "
		"((target_type = 'account' AND target_id = ?) OR (? <> 0 AND target_type = 'character' AND target_id = ?)) ORDER BY id DESC;",
		now, static_cast<int64_t>(accountId), characterId, characterId);
	while (result->next()) grants.push_back(ReadGrant(result));
	return grants;
}

std::vector<IPermissionGrants::Grant> MySQLDatabase::GetRecentPermissionGrants(bool activeOnly, int64_t now, uint32_t limit) {
	std::vector<Grant> grants;
	auto result = activeOnly
		? ExecuteSelect("SELECT * FROM permission_grants WHERE revoked_at = 0 AND (expires_at = 0 OR expires_at > ?) ORDER BY id DESC LIMIT ?;", now, limit)
		: ExecuteSelect("SELECT * FROM permission_grants ORDER BY id DESC LIMIT ?;", limit);
	while (result->next()) grants.push_back(ReadGrant(result));
	return grants;
}

bool MySQLDatabase::RevokePermissionGrant(uint64_t id, const std::string& revokedBy, int64_t time) {
	return ExecuteUpdate("UPDATE permission_grants SET revoked_at = ?, revoked_by = ? WHERE id = ? AND revoked_at = 0;", time, revokedBy, id) > 0;
}
