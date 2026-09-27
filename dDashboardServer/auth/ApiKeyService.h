#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ApiKeyScope.h"
#include "IApiKeys.h"

struct HTTPContext;
struct HTTPReply;

/**
 * Dashboard API keys: checking them on requests, their rate limits and quotas, and their usage counters.
 * A key's effective permissions are its scope AND its owner's current permissions: the owner is looked up on every
 * request, so a ban, lock, demotion or "sign out everywhere" narrows or stops the key at once.
 * Runs on the web thread only.
 */
namespace ApiKeyService {
	constexpr uint32_t DEFAULT_RATE_LIMIT = 120; // requests a minute, unless api_key_rate_limit says otherwise
	constexpr uint32_t MAX_RATE_LIMIT = 6000;
	constexpr uint32_t MAX_DAILY_QUOTA = 10'000'000;

	struct NewSecret {
		std::string token;  // shown to the person once, never stored
		std::string hash;   // SHA-256 hex, stored
		std::string prefix; // the start of the key, stored to tell keys apart
	};
	NewSecret GenerateSecret();

	bool LooksLikeKey(std::string_view token);

	// Paths a key may never use, whatever its scope: signing in, the account's own password, email, two-factor and
	// sessions, and managing API keys (a leaked key must not be able to make itself new keys or lock its owner out)
	bool SessionOnlyPath(std::string_view path);

	enum class eResult : uint8_t {
		INVALID,      // unknown, revoked or expired key, or an owner who can't sign in: the request is unauthenticated
		OK,           // context filled in
		REFUSED,      // reply written (403 for the key's IP/path restrictions, 429 for its limits)
	};
	eResult Authenticate(const std::string& token, HTTPContext& context, HTTPReply& reply);

	// For WebSocket connections: the owner and scope, without counting against the limits
	struct Verified {
		uint32_t accountId{};
		std::string username;
		uint8_t gmLevel{};
		bool needsTwoFactorSetup{};
		std::shared_ptr<const ApiKeys::Scope> scope;
	};
	std::optional<Verified> Verify(const std::string& token);

	// Stop using what is cached about a key (after it is revoked or rotated)
	void Forget(uint64_t keyId);
	void ForgetAll();

	// Requests since the last write, not yet in the database (for the key list)
	std::optional<IApiKeys::ApiKeyUsage> PendingUsage(uint64_t keyId);

	// Write the usage counters now and then (call every tick) and before shutting down
	void Update();
	void Flush();

	// The address a request came from, for the key's allowed addresses (the dashboard's behind_proxy rule)
	void SetClientAddress(std::function<std::string(const HTTPContext&)> resolve);

	// Record that a key was refused something; at most once a minute per key and reason, so a busy script can't flood
	// the audit log
	void SetDeniedHook(std::function<void(const HTTPContext&, const std::string& reason)> hook);
	void NoteDenied(const HTTPContext& context, const std::string& reason);

	uint32_t DefaultRateLimit();
}
