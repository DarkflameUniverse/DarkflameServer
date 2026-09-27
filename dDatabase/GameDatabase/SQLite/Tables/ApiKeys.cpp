#include "SQLiteDatabase.h"

namespace {
	IApiKeys::ApiKey ReadApiKey(CppSQLite3Query& result) {
		IApiKeys::ApiKey key;
		key.id = static_cast<uint64_t>(result.getInt64Field("id"));
		key.accountId = static_cast<uint32_t>(result.getIntField("account_id"));
		key.name = result.getStringField("name");
		key.note = result.getStringField("note");
		key.keyHash = result.getStringField("key_hash");
		key.keyPrefix = result.getStringField("key_prefix");
		key.permissions = result.getStringField("permissions");
		key.readOnly = result.getIntField("read_only") != 0;
		key.allowedIps = result.getStringField("allowed_ips");
		key.allowedPaths = result.getStringField("allowed_paths");
		key.rateLimit = static_cast<uint32_t>(result.getInt64Field("rate_limit"));
		key.dailyQuota = static_cast<uint32_t>(result.getInt64Field("daily_quota"));
		key.createdAt = result.getInt64Field("created_at");
		key.createdBy = result.getStringField("created_by");
		key.issuedAt = result.getInt64Field("issued_at");
		key.expiresAt = result.getInt64Field("expires_at");
		key.revokedAt = result.getInt64Field("revoked_at");
		key.revokedBy = result.getStringField("revoked_by");
		key.lastUsedAt = result.getInt64Field("last_used_at");
		key.lastIp = result.getStringField("last_ip");
		key.requestCount = static_cast<uint64_t>(result.getInt64Field("request_count"));
		key.quotaDay = result.getIntField("quota_day");
		key.dayCount = static_cast<uint32_t>(result.getInt64Field("day_count"));
		return key;
	}
}

uint64_t SQLiteDatabase::InsertApiKey(const ApiKey& key) {
	ExecuteInsert("INSERT INTO dashboard_api_keys (account_id, name, note, key_hash, key_prefix, permissions, read_only, allowed_ips, allowed_paths, "
		"rate_limit, daily_quota, created_at, created_by, issued_at, expires_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		key.accountId, key.name, key.note, key.keyHash, key.keyPrefix, key.permissions, key.readOnly, key.allowedIps, key.allowedPaths,
		key.rateLimit, key.dailyQuota, key.createdAt, key.createdBy, key.issuedAt, key.expiresAt);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("id"));
}

std::optional<IApiKeys::ApiKey> SQLiteDatabase::GetApiKey(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM dashboard_api_keys WHERE id = ?;", static_cast<int64_t>(id));
	if (result.eof()) return std::nullopt;
	return ReadApiKey(result);
}

std::optional<IApiKeys::ApiKey> SQLiteDatabase::GetApiKeyByHash(const std::string& keyHash) {
	auto [_, result] = ExecuteSelect("SELECT * FROM dashboard_api_keys WHERE key_hash = ?;", keyHash);
	if (result.eof()) return std::nullopt;
	return ReadApiKey(result);
}

std::vector<IApiKeys::ApiKey> SQLiteDatabase::GetApiKeys(uint32_t accountId) {
	std::vector<ApiKey> keys;
	auto [_, result] = ExecuteSelect("SELECT * FROM dashboard_api_keys WHERE account_id = ? ORDER BY id DESC;", accountId);
	for (; !result.eof(); result.nextRow()) keys.push_back(ReadApiKey(result));
	return keys;
}

void SQLiteDatabase::RevokeApiKey(uint64_t id, const std::string& revokedBy, int64_t time) {
	ExecuteUpdate("UPDATE dashboard_api_keys SET revoked_at = ?, revoked_by = ? WHERE id = ? AND revoked_at = 0;", time, revokedBy, static_cast<int64_t>(id));
}

uint32_t SQLiteDatabase::RevokeAccountApiKeys(uint32_t accountId, const std::string& revokedBy, int64_t time) {
	return ExecuteUpdate("UPDATE dashboard_api_keys SET revoked_at = ?, revoked_by = ? WHERE account_id = ? AND revoked_at = 0;", time, revokedBy, accountId);
}

void SQLiteDatabase::RotateApiKey(uint64_t id, const std::string& keyHash, const std::string& keyPrefix, int64_t issuedAt) {
	ExecuteUpdate("UPDATE dashboard_api_keys SET key_hash = ?, key_prefix = ?, issued_at = ? WHERE id = ? AND revoked_at = 0;", keyHash, keyPrefix, issuedAt, static_cast<int64_t>(id));
}

void SQLiteDatabase::RecordApiKeyUsage(const std::vector<ApiKeyUsage>& usage) {
	if (usage.empty()) return;
	// One transaction for the batch: one write to disk rather than one per key
	DatabaseTransaction transaction(*this);
	for (const auto& entry : usage) {
		ExecuteUpdate("UPDATE dashboard_api_keys SET request_count = request_count + ?, last_used_at = MAX(last_used_at, ?), last_ip = ?, quota_day = ?, day_count = ? WHERE id = ?;",
			static_cast<int64_t>(entry.requests), entry.lastUsedAt, entry.lastIp, entry.quotaDay, entry.dayCount, static_cast<int64_t>(entry.id));
	}
	transaction.Commit();
}
