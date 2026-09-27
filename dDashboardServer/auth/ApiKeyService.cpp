#include "ApiKeyService.h"

#include <chrono>
#include <ctime>
#include <map>
#include <mutex>
#include <unordered_map>

#include <openssl/rand.h>

#include "ApiKeyLimiter.h"
#include "Database.h"
#include "DashboardAuthService.h"
#include "dConfig.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "HTTPContext.h"
#include "HTTPReply.h"
#include "Logger.h"

namespace {
	using Clock = std::chrono::steady_clock;
	// How long what the database says about a key is trusted. Revoking and rotating on this server take effect at
	// once (Forget); the owner's account is looked up on every request regardless.
	constexpr auto CACHE_TTL = std::chrono::seconds(30);
	constexpr size_t CACHE_MAX = 10000;
	constexpr auto FLUSH_INTERVAL = std::chrono::seconds(60);
	constexpr auto DENIED_THROTTLE = std::chrono::seconds(60);
	constexpr auto LIMITER_IDLE = std::chrono::minutes(30);

	struct CachedKey {
		std::optional<IApiKeys::ApiKey> key; // nullopt: no such key (so a stream of bad keys doesn't hit the database)
		std::shared_ptr<const ApiKeys::Scope> scope;
		std::vector<std::string> allowedIps;
		std::vector<std::string> allowedPaths;
		Clock::time_point loadedAt;
	};

	struct State {
		std::recursive_mutex mutex;
		std::unordered_map<std::string, CachedKey> cache; // by key hash
		ApiKeyLimiter limiter;
		std::map<uint64_t, IApiKeys::ApiKeyUsage> pending;
		std::map<std::pair<uint64_t, std::string>, Clock::time_point> deniedAt;
		Clock::time_point nextFlush = Clock::now() + FLUSH_INTERVAL;
		std::function<std::string(const HTTPContext&)> clientAddress;
		std::function<void(const HTTPContext&, const std::string&)> deniedHook;
	};

	State& GetState() {
		static State state;
		return state;
	}

	const CachedKey& Lookup(State& state, const std::string& hash) {
		const auto now = Clock::now();
		auto it = state.cache.find(hash);
		if (it != state.cache.end() && now - it->second.loadedAt < CACHE_TTL) return it->second;
		if (state.cache.size() >= CACHE_MAX) state.cache.clear();

		CachedKey entry;
		entry.loadedAt = now;
		entry.key = Database::Get()->GetApiKeyByHash(hash);
		if (entry.key) {
			auto scope = std::make_shared<ApiKeys::Scope>();
			scope->keyId = entry.key->id;
			scope->name = entry.key->name;
			scope->readOnly = entry.key->readOnly;
			ApiKeys::ParsePermissions(entry.key->permissions, scope->allPermissions, scope->permissions);
			entry.scope = std::move(scope);
			entry.allowedIps = ApiKeys::SplitList(entry.key->allowedIps);
			entry.allowedPaths = ApiKeys::SplitList(entry.key->allowedPaths);
		}
		return state.cache[hash] = std::move(entry);
	}

	struct Owner {
		std::string username;
		uint8_t gmLevel{};
	};

	// The key is in use and its owner may still sign in; the owner's GM level as of now
	std::optional<Owner> CheckKeyAndOwner(const IApiKeys::ApiKey& key) {
		const auto now = static_cast<int64_t>(std::time(nullptr));
		if (key.revokedAt != 0 || (key.expiresAt != 0 && key.expiresAt <= now)) return std::nullopt;
		const auto account = Database::Get()->GetAccountById(key.accountId);
		if (account.contains("error") || account.value("banned", false) || account.value("locked", false)) return std::nullopt;
		const auto gmLevel = static_cast<uint8_t>(account.value("gm_level", 0));
		if (!DashboardAuthService::HasDashboardAccess(gmLevel)) return std::nullopt;
		// "Sign out everywhere" and password resets stop keys made before them, as they stop sessions
		if (key.issuedAt < Database::Get()->GetSessionsValidAfter(key.accountId)) return std::nullopt;
		return Owner{ account.value("name", std::string{}), gmLevel };
	}

	std::string Hex(const unsigned char* bytes, size_t size) {
		static constexpr char digits[] = "0123456789abcdef";
		std::string out;
		out.reserve(size * 2);
		for (size_t i = 0; i < size; ++i) {
			out += digits[bytes[i] >> 4];
			out += digits[bytes[i] & 15];
		}
		return out;
	}

	void Refuse(HTTPReply& reply, eHTTPStatusCode status, const std::string& message) {
		reply.status = status;
		reply.contentType = eContentType::APPLICATION_JSON;
		reply.message = nlohmann::json{ {"success", false}, {"error", message} }.dump();
	}
}

namespace ApiKeyService {
	NewSecret GenerateSecret() {
		unsigned char bytes[32];
		if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("RAND_bytes failed");
		NewSecret secret;
		secret.token = std::string(ApiKeys::TOKEN_PREFIX) + Hex(bytes, sizeof(bytes));
		secret.hash = DashboardAuthService::Sha256Hex(secret.token);
		secret.prefix = secret.token.substr(0, ApiKeys::TOKEN_PREFIX.size() + 6);
		return secret;
	}

	bool LooksLikeKey(std::string_view token) {
		return token.starts_with(ApiKeys::TOKEN_PREFIX);
	}

	bool SessionOnlyPath(std::string_view path) {
		if (path.starts_with("/api/auth/")) return true;
		if (path.starts_with("/api/account/") && path != "/api/account/permissions") return true;
		return path.find("/api_keys") != std::string_view::npos;
	}

	uint32_t DefaultRateLimit() {
		if (!Game::config) return DEFAULT_RATE_LIMIT;
		const auto value = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("api_key_rate_limit"));
		return value && *value > 0 ? std::min(*value, MAX_RATE_LIMIT) : DEFAULT_RATE_LIMIT;
	}

	eResult Authenticate(const std::string& token, HTTPContext& context, HTTPReply& reply) {
		auto& state = GetState();
		std::lock_guard lock(state.mutex);
		const auto& cached = Lookup(state, DashboardAuthService::Sha256Hex(token));
		if (!cached.key) return eResult::INVALID;
		const auto& key = *cached.key;
		const auto owner = CheckKeyAndOwner(key);
		if (!owner) return eResult::INVALID;

		context.isAuthenticated = true;
		context.authenticatedUser = owner->username;
		context.accountId = key.accountId;
		context.gmLevel = owner->gmLevel;
		context.apiKey = cached.scope;
		context.userData["auth_source"] = "header";
		context.userData["api_key"] = key.name;
		if (DashboardAuthService::NeedsTwoFactorSetup(key.accountId, owner->gmLevel)) context.userData["needs_2fa"] = "1";

		const auto address = state.clientAddress ? state.clientAddress(context) : context.clientIP;
		if (!ApiKeys::AddressAllowed(cached.allowedIps, address)) {
			NoteDenied(context, "address " + address + " isn't allowed");
			Refuse(reply, eHTTPStatusCode::FORBIDDEN, "This API key can't be used from this address");
			return eResult::REFUSED;
		}
		if (SessionOnlyPath(context.path)) {
			NoteDenied(context, context.method + " " + context.path + " needs a signed-in browser session");
			Refuse(reply, eHTTPStatusCode::FORBIDDEN, "API keys can't be used for this; sign in to the dashboard instead");
			return eResult::REFUSED;
		}
		if (!ApiKeys::PathAllowed(cached.allowedPaths, context.path)) {
			NoteDenied(context, context.method + " " + context.path + " is outside its allowed paths");
			Refuse(reply, eHTTPStatusCode::FORBIDDEN, "This API key isn't allowed to use this path");
			return eResult::REFUSED;
		}

		const auto now = std::time(nullptr);
		const auto day = static_cast<int32_t>(now / 86400);
		const auto toNextDay = static_cast<uint32_t>(86400 - now % 86400);
		const auto perMinute = key.rateLimit > 0 ? std::min(key.rateLimit, MAX_RATE_LIMIT) : DefaultRateLimit();
		const auto decision = state.limiter.Check(key.id, perMinute, key.dailyQuota, day, toNextDay, key.quotaDay, key.dayCount, Clock::now());
		reply.headers.push_back("X-RateLimit-Limit: " + std::to_string(decision.limit));
		reply.headers.push_back("X-RateLimit-Remaining: " + std::to_string(decision.remaining));
		if (decision.quota > 0) {
			reply.headers.push_back("X-Quota-Limit: " + std::to_string(decision.quota));
			reply.headers.push_back("X-Quota-Remaining: " + std::to_string(decision.quotaRemaining));
		}
		if (!decision.allowed) {
			reply.headers.push_back("Retry-After: " + std::to_string(decision.retryAfterSeconds));
			NoteDenied(context, decision.quotaExceeded ? "its daily quota is used up" : "it went over its rate limit");
			Refuse(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, decision.quotaExceeded
				? "This API key has used up its daily quota" : "This API key is making too many requests; slow down");
			return eResult::REFUSED;
		}

		auto& usage = state.pending[key.id];
		usage.id = key.id;
		usage.requests++;
		usage.lastUsedAt = now;
		usage.lastIp = address.substr(0, 64);
		usage.quotaDay = day;
		usage.dayCount = decision.dayCount;
		return eResult::OK;
	}

	std::optional<Verified> Verify(const std::string& token) {
		auto& state = GetState();
		std::lock_guard lock(state.mutex);
		const auto& cached = Lookup(state, DashboardAuthService::Sha256Hex(token));
		if (!cached.key) return std::nullopt;
		const auto owner = CheckKeyAndOwner(*cached.key);
		if (!owner) return std::nullopt;
		return Verified{ cached.key->accountId, owner->username, owner->gmLevel,
			DashboardAuthService::NeedsTwoFactorSetup(cached.key->accountId, owner->gmLevel), cached.scope };
	}

	void Forget(uint64_t keyId) {
		auto& state = GetState();
		std::lock_guard lock(state.mutex);
		std::erase_if(state.cache, [keyId](const auto& entry) { return entry.second.key && entry.second.key->id == keyId; });
		state.limiter.Forget(keyId);
	}

	void ForgetAll() {
		auto& state = GetState();
		std::lock_guard lock(state.mutex);
		state.cache.clear();
	}

	std::optional<IApiKeys::ApiKeyUsage> PendingUsage(uint64_t keyId) {
		auto& state = GetState();
		std::lock_guard lock(state.mutex);
		const auto it = state.pending.find(keyId);
		if (it == state.pending.end()) return std::nullopt;
		return it->second;
	}

	void Flush() {
		auto& state = GetState();
		std::lock_guard lock(state.mutex);
		state.nextFlush = Clock::now() + FLUSH_INTERVAL;
		if (state.pending.empty()) return;
		std::vector<IApiKeys::ApiKeyUsage> batch;
		batch.reserve(state.pending.size());
		for (const auto& [_, usage] : state.pending) batch.push_back(usage);
		try {
			Database::Get()->RecordApiKeyUsage(batch);
			state.pending.clear();
		} catch (const std::exception& ex) {
			LOG("Failed to save API key usage: %s", ex.what());
		}
	}

	void Update() {
		auto& state = GetState();
		std::lock_guard lock(state.mutex);
		const auto now = Clock::now();
		if (now < state.nextFlush) return;
		Flush();
		state.limiter.Prune(now, LIMITER_IDLE);
		std::erase_if(state.deniedAt, [&](const auto& entry) { return now - entry.second >= DENIED_THROTTLE; });
	}

	void SetClientAddress(std::function<std::string(const HTTPContext&)> resolve) {
		GetState().clientAddress = std::move(resolve);
	}

	void SetDeniedHook(std::function<void(const HTTPContext&, const std::string& reason)> hook) {
		GetState().deniedHook = std::move(hook);
	}

	void NoteDenied(const HTTPContext& context, const std::string& reason) {
		if (!context.apiKey) return;
		auto& state = GetState();
		std::lock_guard lock(state.mutex);
		const auto now = Clock::now();
		auto [it, inserted] = state.deniedAt.try_emplace({ context.apiKey->keyId, reason }, now);
		if (!inserted) {
			if (now - it->second < DENIED_THROTTLE) return;
			it->second = now;
		}
		LOG("API key %llu (%s) of %s refused: %s", static_cast<unsigned long long>(context.apiKey->keyId), context.apiKey->name.c_str(),
			context.authenticatedUser.c_str(), reason.c_str());
		if (state.deniedHook) state.deniedHook(context, reason);
	}
}
