#ifndef __IAPIKEYS__H__
#define __IAPIKEYS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * Dashboard API keys (ApiKeyScope.h). Only a hash of each key is stored. The usage columns are written in batches
 * (RecordApiKeyUsage) by the dashboard, not on every request.
 */
class IApiKeys {
public:
	struct ApiKey {
		uint64_t id{};
		uint32_t accountId{};
		std::string name;
		std::string note;
		std::string keyHash;       // SHA-256 hex of the whole key
		std::string keyPrefix;     // the start of the key, to tell keys apart
		std::string permissions;   // "*" or comma-separated permission names
		bool readOnly{};
		std::string allowedIps;    // comma-separated; empty: any
		std::string allowedPaths;  // comma-separated path prefixes; empty: any
		uint32_t rateLimit{};      // requests a minute; 0: the server's default
		uint32_t dailyQuota{};     // requests a UTC day; 0: none
		int64_t createdAt{};
		std::string createdBy;
		int64_t issuedAt{};        // when the current secret was made
		int64_t expiresAt{};       // 0: never
		int64_t revokedAt{};       // 0: in use
		std::string revokedBy;
		int64_t lastUsedAt{};
		std::string lastIp;
		uint64_t requestCount{};
		int32_t quotaDay{};        // the UTC day (days since 1970) dayCount counts
		uint32_t dayCount{};
	};

	// Usage since the last flush, for one key
	struct ApiKeyUsage {
		uint64_t id{};
		uint64_t requests{};       // added to request_count
		int64_t lastUsedAt{};
		std::string lastIp;
		int32_t quotaDay{};
		uint32_t dayCount{};       // replaces day_count (the count for quotaDay so far)
	};

	virtual uint64_t InsertApiKey(const ApiKey& key) = 0;
	virtual std::optional<ApiKey> GetApiKey(uint64_t id) = 0;
	virtual std::optional<ApiKey> GetApiKeyByHash(const std::string& keyHash) = 0;
	// Newest first, revoked ones included
	virtual std::vector<ApiKey> GetApiKeys(uint32_t accountId) = 0;
	virtual void RevokeApiKey(uint64_t id, const std::string& revokedBy, int64_t time) = 0;
	// Every key of the account still in use; returns how many were revoked
	virtual uint32_t RevokeAccountApiKeys(uint32_t accountId, const std::string& revokedBy, int64_t time) = 0;
	// A new secret for the key (the old one stops working)
	virtual void RotateApiKey(uint64_t id, const std::string& keyHash, const std::string& keyPrefix, int64_t issuedAt) = 0;
	virtual void RecordApiKeyUsage(const std::vector<ApiKeyUsage>& usage) = 0;
};

#endif  //!__IAPIKEYS__H__
