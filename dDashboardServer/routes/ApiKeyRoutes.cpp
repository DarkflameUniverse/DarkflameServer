#include "ApiKeyRoutes.h"

#include <algorithm>
#include <ctime>

#include "AccountRules.h"
#include "ApiKeyScope.h"
#include "ApiKeyService.h"
#include "Database.h"
#include "eHTTPMethod.h"
#include "GeneralUtils.h"
#include "HTTPContext.h"
#include "Permissions.h"
#include "RequireAuthMiddleware.h"
#include "RouteUtils.h"

using namespace RouteUtils;
using AccountRules::eAccountAction;

namespace {
	constexpr size_t MAX_NAME = 32;
	constexpr size_t MAX_NOTE = 255;
	constexpr size_t MAX_LIST = 512;
	constexpr size_t MAX_ACTIVE_KEYS = 25;
	constexpr int64_t MAX_DAYS = 3650;

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	bool FromSession(const HTTPContext& context) {
		const auto source = context.userData.find("auth_source");
		return !context.apiKey && source != context.userData.end() && source->second == "cookie";
	}

	std::string Trim(std::string text) {
		text.erase(0, text.find_first_not_of(" \t\r\n"));
		text.erase(text.find_last_not_of(" \t\r\n") + 1);
		return text;
	}

	// A comma-separated list checked entry by entry; nullopt if an entry isn't allowed
	std::optional<std::string> CleanList(const std::string& text, bool paths) {
		std::string out;
		for (auto entry : ApiKeys::SplitList(text)) {
			if (paths) {
				std::ranges::transform(entry, entry.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (!entry.starts_with('/') || entry.find_first_of(" ,\"'<>") != std::string::npos) return std::nullopt;
			} else if (!std::ranges::all_of(entry, [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) || c == '.' || c == ':'; })) {
				return std::nullopt;
			}
			if (!out.empty()) out += ',';
			out += entry;
		}
		if (out.size() > MAX_LIST) return std::nullopt;
		return out;
	}

	std::string KeyStatus(const IApiKeys::ApiKey& key, int64_t sessionsValidAfter, int64_t now) {
		if (key.revokedAt != 0) return "revoked";
		if (key.expiresAt != 0 && key.expiresAt <= now) return "expired";
		if (key.issuedAt < sessionsValidAfter) return "signed_out";
		return "active";
	}

	nlohmann::json KeyJson(const IApiKeys::ApiKey& key, uint8_t ownerLevel, int64_t sessionsValidAfter) {
		const auto now = Now();
		bool all = false;
		std::set<std::string> permissions;
		ApiKeys::ParsePermissions(key.permissions, all, permissions);
		// What the key names that its owner can't do any more (a demotion or a changed permission): it doesn't work
		nlohmann::json lost = nlohmann::json::array();
		for (const auto& permission : permissions) if (!Permissions::Allowed(ownerLevel, permission)) lost.push_back(permission);

		auto requests = key.requestCount;
		auto lastUsed = key.lastUsedAt;
		auto lastIp = key.lastIp;
		const auto today = static_cast<int32_t>(now / 86400);
		uint32_t todayCount = key.quotaDay == today ? key.dayCount : 0;
		if (const auto pending = ApiKeyService::PendingUsage(key.id)) {
			requests += pending->requests;
			if (pending->lastUsedAt >= lastUsed) {
				lastUsed = pending->lastUsedAt;
				lastIp = pending->lastIp;
			}
			if (pending->quotaDay == today) todayCount = pending->dayCount;
		}
		return {
			{"id", key.id}, {"accountId", key.accountId}, {"name", key.name}, {"note", key.note}, {"prefix", key.keyPrefix},
			{"allPermissions", all}, {"permissions", permissions}, {"lostPermissions", lost}, {"readOnly", key.readOnly},
			{"allowedIps", key.allowedIps}, {"allowedPaths", key.allowedPaths},
			{"rateLimit", key.rateLimit}, {"effectiveRateLimit", key.rateLimit > 0 ? key.rateLimit : ApiKeyService::DefaultRateLimit()},
			{"dailyQuota", key.dailyQuota}, {"todayCount", todayCount},
			{"createdAt", key.createdAt}, {"createdBy", key.createdBy}, {"issuedAt", key.issuedAt}, {"expiresAt", key.expiresAt},
			{"revokedAt", key.revokedAt}, {"revokedBy", key.revokedBy},
			{"lastUsedAt", lastUsed}, {"lastIp", lastIp}, {"requestCount", requests},
			{"status", KeyStatus(key, sessionsValidAfter, now)},
		};
	}

	std::string Describe(const IApiKeys::ApiKey& key) {
		std::string text = "'" + key.name + "' (" + key.keyPrefix + "...): ";
		text += key.permissions == ApiKeys::ALL_PERMISSIONS ? "all of their permissions" : (key.permissions.empty() ? "no permissions" : key.permissions);
		if (key.readOnly) text += "; read-only";
		if (!key.allowedIps.empty()) text += "; from " + key.allowedIps;
		if (!key.allowedPaths.empty()) text += "; paths " + key.allowedPaths;
		text += "; " + (key.rateLimit > 0 ? std::to_string(key.rateLimit) : "default") + " requests/min";
		if (key.dailyQuota > 0) text += "; " + std::to_string(key.dailyQuota) + " a day";
		text += key.expiresAt > 0 ? "; expires " + std::to_string(key.expiresAt) : "; no expiry";
		return text;
	}

	uint32_t ActiveKeyCount(uint32_t accountId, int64_t sessionsValidAfter) {
		const auto now = Now();
		const auto keys = Database::Get()->GetApiKeys(accountId);
		return static_cast<uint32_t>(std::ranges::count_if(keys, [&](const auto& key) { return KeyStatus(key, sessionsValidAfter, now) == "active"; }));
	}

	// The key, if the signed-in person may act on it: their own, or (to see or revoke) another account's with
	// api_keys_manage under the rank rules. Writes the error reply otherwise.
	std::optional<IApiKeys::ApiKey> KeyForAction(const HTTPContext& context, HTTPReply& reply, bool ownOnly) {
		const auto id = PathId<uint64_t>(context.path, 2);
		const auto key = id ? Database::Get()->GetApiKey(*id) : std::nullopt;
		if (!key) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "API key not found");
			return std::nullopt;
		}
		if (key->accountId == context.accountId) return key;
		if (ownOnly || !Can(context, "api_keys_manage")) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "API key not found");
			return std::nullopt;
		}
		if (!AuthorizeAccountAction(context, key->accountId, reply, eAccountAction::TOOLS)) return std::nullopt;
		return key;
	}

	IApiKeys::ApiKey NewKey(const HTTPContext& context, const ApiKeyService::NewSecret& secret) {
		IApiKeys::ApiKey key;
		key.accountId = context.accountId;
		key.keyHash = secret.hash;
		key.keyPrefix = secret.prefix;
		key.createdAt = key.issuedAt = Now();
		key.createdBy = context.authenticatedUser;
		return key;
	}
}

namespace ApiKeyRoutes {
	std::string CreateFullKey(const HTTPContext& context, const std::string& name, int64_t days) {
		const auto secret = ApiKeyService::GenerateSecret();
		auto key = NewKey(context, secret);
		key.name = name;
		key.permissions = std::string(ApiKeys::ALL_PERMISSIONS);
		key.expiresAt = days > 0 ? key.createdAt + std::clamp<int64_t>(days, 1, MAX_DAYS) * 86400 : 0;
		key.id = Database::Get()->InsertApiKey(key);
		Audit(context, "create_api_key", "Made API key " + Describe(key), AuditTarget::Account(context.accountId));
		return secret.token;
	}

	void RegisterRoutes() {
		// Keys only ever narrow their owner, so the caller's own permissions are what may be picked
		Route(eHTTPMethod::GET, "/api/api_keys/permissions", 0,
			"The permissions an API key can be given, grouped like the Permissions page; 'allowed' marks the ones you have (only those can be picked)",
			[](HTTPReply& reply, const HTTPContext& context) {
				nlohmann::json permissions = nlohmann::json::array();
				for (const auto& permission : Permissions::All()) {
					permissions.push_back({ {"key", permission.key}, {"category", permission.category}, {"title", permission.title},
						{"description", permission.description}, {"allowed", Permissions::Allowed(context.gmLevel, permission.key)} });
				}
				JsonSuccess(reply, { {"permissions", permissions}, {"defaultRateLimit", ApiKeyService::DefaultRateLimit()},
					{"maxRateLimit", ApiKeyService::MAX_RATE_LIMIT}, {"maxDailyQuota", ApiKeyService::MAX_DAILY_QUOTA},
					{"apiAccess", Permissions::Allowed(context.gmLevel, "api_access")} });
			});

		Route(eHTTPMethod::GET, "/api/accounts/:id/api_keys", 0,
			"An account's API keys, newest first (never the keys themselves). Your own, or anyone's you may manage with api_keys_manage",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = PathId<uint32_t>(context.path, 2);
				if (!accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid account ID");
				const bool own = *accountId == context.accountId;
				if (!own) {
					if (!Can(context, "api_keys_manage") || !Can(context, "accounts_view")) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Insufficient permissions");
					if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::TOOLS)) return;
				}
				const auto account = Database::Get()->GetAccountById(*accountId);
				if (account.contains("error")) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Account not found");
				const auto ownerLevel = static_cast<uint8_t>(account.value("gm_level", 0));
				const auto validAfter = Database::Get()->GetSessionsValidAfter(*accountId);
				nlohmann::json keys = nlohmann::json::array();
				for (const auto& key : Database::Get()->GetApiKeys(*accountId)) keys.push_back(KeyJson(key, ownerLevel, validAfter));
				JsonSuccess(reply, { {"keys", keys}, {"own", own} });
			});

		Route(eHTTPMethod::POST, "/api/api_keys", 0,
			"Make an API key (signed in with the browser only). Body: {name, note, permissions: [names] or \"*\" (all of yours), readOnly, "
			"allowedIps, allowedPaths (comma-separated), rateLimit (a minute, 0: default), dailyQuota (0: none), expiresInDays (0: never)}. "
			"Returns {key}, shown only this once",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!FromSession(context)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Make API keys from your account page while signed in");
				if (!Can(context, "api_access")) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "API access isn't allowed for your account");
				const auto body = ParseBody(context);
				if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");

				const auto secret = ApiKeyService::GenerateSecret();
				auto key = NewKey(context, secret);
				key.name = Trim(body->value("name", ""));
				if (key.name.empty() || key.name.size() > MAX_NAME) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Give the key a name of up to 32 characters");
				key.note = Trim(body->value("note", ""));
				if (key.note.size() > MAX_NOTE) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The note is too long");

				const auto& requested = (*body)["permissions"];
				if (requested.is_string() && requested.get<std::string>() == ApiKeys::ALL_PERMISSIONS) {
					key.permissions = std::string(ApiKeys::ALL_PERMISSIONS);
				} else if (requested.is_array()) {
					std::set<std::string> permissions;
					for (const auto& permission : requested) if (permission.is_string()) permissions.insert(permission.get<std::string>());
					if (permissions.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick at least one permission");
					// Staff can't hand a key more than they have
					const auto refused = Permissions::NotGrantable(context.gmLevel, permissions);
					if (!refused.empty()) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You can't give a key permissions you don't have: " + *refused.begin());
					key.permissions = ApiKeys::JoinPermissions(false, permissions);
				} else {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "permissions must be a list of permission names or \"*\"");
				}

				key.readOnly = body->value("readOnly", false);
				const auto ips = CleanList(body->value("allowedIps", ""), false);
				if (!ips) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Allowed addresses must be IP addresses or prefixes like 10.0.0., separated by commas");
				const auto paths = CleanList(body->value("allowedPaths", ""), true);
				if (!paths) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Allowed paths must start with / and be separated by commas");
				key.allowedIps = *ips;
				key.allowedPaths = *paths;
				const auto rate = body->value("rateLimit", int64_t{ 0 });
				const auto quota = body->value("dailyQuota", int64_t{ 0 });
				const auto days = body->value("expiresInDays", int64_t{ 0 });
				if (rate < 0 || rate > ApiKeyService::MAX_RATE_LIMIT) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The rate limit must be 0 to " + std::to_string(ApiKeyService::MAX_RATE_LIMIT) + " a minute");
				if (quota < 0 || quota > ApiKeyService::MAX_DAILY_QUOTA) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The daily quota is out of range");
				if (days < 0 || days > MAX_DAYS) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Expiry must be 0 (never) to 3650 days");
				key.rateLimit = static_cast<uint32_t>(rate);
				key.dailyQuota = static_cast<uint32_t>(quota);
				key.expiresAt = days > 0 ? key.createdAt + days * 86400 : 0;

				if (ActiveKeyCount(context.accountId, Database::Get()->GetSessionsValidAfter(context.accountId)) >= MAX_ACTIVE_KEYS) {
					return JsonError(reply, eHTTPStatusCode::CONFLICT, "You have too many API keys; revoke some first");
				}
				key.id = Database::Get()->InsertApiKey(key);
				Audit(context, "create_api_key", "Made API key " + Describe(key), AuditTarget::Account(context.accountId));
				JsonSuccess(reply, { {"id", key.id}, {"key", secret.token}, {"message", "API key made - copy it now, it won't be shown again"} });
			});

		Route(eHTTPMethod::POST, "/api/api_keys/:id/revoke", 0,
			"Revoke an API key: yours, or another account's with api_keys_manage (the rank rules apply)",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!FromSession(context)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Revoke API keys from the dashboard while signed in");
				const auto key = KeyForAction(context, reply, false);
				if (!key) return;
				if (key->revokedAt != 0) return JsonError(reply, eHTTPStatusCode::CONFLICT, "This key is already revoked");
				Database::Get()->RevokeApiKey(key->id, context.authenticatedUser, Now());
				ApiKeyService::Forget(key->id);
				Audit(context, "revoke_api_key", "Revoked API key '" + key->name + "' (" + key->keyPrefix + "...)", AuditTarget::Account(key->accountId));
				JsonSuccess(reply, { {"message", "API key revoked"} });
			});

		Route(eHTTPMethod::POST, "/api/api_keys/:id/rotate", 0,
			"Give one of your API keys a new secret, keeping its name, permissions and limits; the old secret stops working. Returns {key}, shown once",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!FromSession(context)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Rotate API keys from your account page while signed in");
				if (!Can(context, "api_access")) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "API access isn't allowed for your account");
				const auto key = KeyForAction(context, reply, true);
				if (!key) return;
				if (key->revokedAt != 0) return JsonError(reply, eHTTPStatusCode::CONFLICT, "A revoked key can't be rotated");
				if (key->expiresAt != 0 && key->expiresAt <= Now()) return JsonError(reply, eHTTPStatusCode::CONFLICT, "An expired key can't be rotated; make a new one");
				// A scope the owner has since lost stays in the key but keeps not working; they can't regain it by rotating
				const auto secret = ApiKeyService::GenerateSecret();
				Database::Get()->RotateApiKey(key->id, secret.hash, secret.prefix, Now());
				ApiKeyService::Forget(key->id);
				Audit(context, "rotate_api_key", "Rotated API key '" + key->name + "' (" + key->keyPrefix + "... is now " + secret.prefix + "...)",
					AuditTarget::Account(key->accountId));
				JsonSuccess(reply, { {"key", secret.token}, {"message", "New secret made - copy it now, it won't be shown again"} });
			});

		// Refusals of keys (their scope, read-only, addresses, paths, limits) go to the audit log, a minute apart at most
		ApiKeyService::SetDeniedHook([](const HTTPContext& context, const std::string& reason) {
			Audit(context, "api_key_denied", reason, AuditTarget::Account(context.accountId));
		});
		RequireAuthMiddleware::SetApiKeyDeniedHook([](const HTTPContext& context, const std::string& reason) { ApiKeyService::NoteDenied(context, reason); });
		ApiKeyService::SetClientAddress([](const HTTPContext& context) { return ClientAddress(context); });
	}
}
