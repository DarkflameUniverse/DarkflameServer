#include "MySQLDatabase.h"

namespace {
	template<typename Result> IApiKeys::ApiKey ReadApiKey(Result& result) {
		IApiKeys::ApiKey key;
		key.id = result->getUInt64("id");
		key.accountId = result->getUInt("account_id");
		key.name = result->getString("name").c_str();
		key.note = result->getString("note").c_str();
		key.keyHash = result->getString("key_hash").c_str();
		key.keyPrefix = result->getString("key_prefix").c_str();
		key.permissions = result->getString("permissions").c_str();
		key.readOnly = result->getInt("read_only") != 0;
		key.allowedIps = result->getString("allowed_ips").c_str();
		key.allowedPaths = result->getString("allowed_paths").c_str();
		key.rateLimit = result->getUInt("rate_limit");
		key.dailyQuota = result->getUInt("daily_quota");
		key.createdAt = result->getInt64("created_at");
		key.createdBy = result->getString("created_by").c_str();
		key.issuedAt = result->getInt64("issued_at");
		key.expiresAt = result->getInt64("expires_at");
		key.revokedAt = result->getInt64("revoked_at");
		key.revokedBy = result->getString("revoked_by").c_str();
		key.lastUsedAt = result->getInt64("last_used_at");
		key.lastIp = result->getString("last_ip").c_str();
		key.requestCount = result->getUInt64("request_count");
		key.quotaDay = result->getInt("quota_day");
		key.dayCount = result->getUInt("day_count");
		return key;
	}
}

uint64_t MySQLDatabase::InsertApiKey(const ApiKey& key) {
	ExecuteInsert("INSERT INTO dashboard_api_keys (account_id, name, note, key_hash, key_prefix, permissions, read_only, allowed_ips, allowed_paths, "
		"rate_limit, daily_quota, created_at, created_by, issued_at, expires_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		key.accountId, key.name, key.note, key.keyHash, key.keyPrefix, key.permissions, key.readOnly, key.allowedIps, key.allowedPaths,
		key.rateLimit, key.dailyQuota, key.createdAt, key.createdBy, key.issuedAt, key.expiresAt);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getUInt64("id") : 0;
}

std::optional<IApiKeys::ApiKey> MySQLDatabase::GetApiKey(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM dashboard_api_keys WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return ReadApiKey(result);
}

std::optional<IApiKeys::ApiKey> MySQLDatabase::GetApiKeyByHash(const std::string& keyHash) {
	auto result = ExecuteSelect("SELECT * FROM dashboard_api_keys WHERE key_hash = ?;", keyHash);
	if (!result->next()) return std::nullopt;
	return ReadApiKey(result);
}

std::vector<IApiKeys::ApiKey> MySQLDatabase::GetApiKeys(uint32_t accountId) {
	std::vector<ApiKey> keys;
	auto result = ExecuteSelect("SELECT * FROM dashboard_api_keys WHERE account_id = ? ORDER BY id DESC;", accountId);
	while (result->next()) keys.push_back(ReadApiKey(result));
	return keys;
}

void MySQLDatabase::RevokeApiKey(uint64_t id, const std::string& revokedBy, int64_t time) {
	ExecuteUpdate("UPDATE dashboard_api_keys SET revoked_at = ?, revoked_by = ? WHERE id = ? AND revoked_at = 0;", time, revokedBy, id);
}

uint32_t MySQLDatabase::RevokeAccountApiKeys(uint32_t accountId, const std::string& revokedBy, int64_t time) {
	return ExecuteUpdate("UPDATE dashboard_api_keys SET revoked_at = ?, revoked_by = ? WHERE account_id = ? AND revoked_at = 0;", time, revokedBy, accountId);
}

void MySQLDatabase::RotateApiKey(uint64_t id, const std::string& keyHash, const std::string& keyPrefix, int64_t issuedAt) {
	ExecuteUpdate("UPDATE dashboard_api_keys SET key_hash = ?, key_prefix = ?, issued_at = ? WHERE id = ? AND revoked_at = 0;", keyHash, keyPrefix, issuedAt, id);
}

void MySQLDatabase::RecordApiKeyUsage(const std::vector<ApiKeyUsage>& usage) {
	if (usage.empty()) return;
	DatabaseTransaction transaction(*this);
	for (const auto& entry : usage) {
		ExecuteUpdate("UPDATE dashboard_api_keys SET request_count = request_count + ?, last_used_at = GREATEST(last_used_at, ?), last_ip = ?, quota_day = ?, day_count = ? WHERE id = ?;",
			entry.requests, entry.lastUsedAt, entry.lastIp, entry.quotaDay, entry.dayCount, entry.id);
	}
	transaction.Commit();
}
