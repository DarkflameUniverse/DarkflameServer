#include "RequireAuthMiddleware.h"
#include "HTTPContext.h"
#include "Web.h"
#include "Game.h"
#include "Logger.h"
#include "Permissions.h"

namespace {
	bool IsApiRequest(const HTTPContext& context) {
		return context.path.starts_with("/api/");
	}

	// What an account that still has to set up two-factor login may reach: signing in and out, its own account
	bool AllowedBeforeTwoFactor(const HTTPContext& context) {
		const auto& path = context.path;
		return path.starts_with("/api/auth/") || path.starts_with("/api/account/") || path == "/account" || path == "/api/status" ||
			path == "/accounts/" + std::to_string(context.accountId) || path == "/api/accounts/" + std::to_string(context.accountId) ||
			path.starts_with("/js/") || path.starts_with("/css/") || path == "/favicon.ico";
	}

	std::function<bool(const HTTPContext&)> g_ApiAccessAllowed;
	std::function<void(const HTTPContext&, HTTPReply&)> g_ForbiddenPage;
	std::function<void(const HTTPContext&, const std::string&)> g_ApiKeyDenied;

	bool RefuseApiKey(const HTTPContext& context, HTTPReply& reply, const std::string& reason, const std::string& message) {
		if (g_ApiKeyDenied) g_ApiKeyDenied(context, reason);
		reply.status = eHTTPStatusCode::FORBIDDEN;
		reply.message = nlohmann::json{ {"success", false}, {"error", message}, {"code", "api_key_scope"} }.dump();
		reply.contentType = eContentType::APPLICATION_JSON;
		return false;
	}

	bool IsSafeMethod(const HTTPContext& context) {
		return context.method == "GET" || context.method == "HEAD" || context.method == "OPTIONS";
	}
}

void RequireAuthMiddleware::SetApiAccessCheck(std::function<bool(const HTTPContext& context)> check) {
	g_ApiAccessAllowed = std::move(check);
}

void RequireAuthMiddleware::SetForbiddenPage(std::function<void(const HTTPContext& context, HTTPReply& reply)> render) {
	g_ForbiddenPage = std::move(render);
}

RequireAuthMiddleware::RequireAuthMiddleware(uint8_t minGmLevel) : requiredLevel([minGmLevel] { return minGmLevel; }) {}

RequireAuthMiddleware::RequireAuthMiddleware(std::function<uint8_t()> requiredLevel) : requiredLevel(std::move(requiredLevel)) {}

RequireAuthMiddleware::RequireAuthMiddleware(std::function<uint8_t()> requiredLevel, std::string permission)
	: requiredLevel(std::move(requiredLevel)), permission(std::move(permission)) {}

void RequireAuthMiddleware::SetApiKeyDeniedHook(std::function<void(const HTTPContext& context, const std::string& reason)> hook) {
	g_ApiKeyDenied = std::move(hook);
}

bool RequireAuthMiddleware::Process(HTTPContext& context, HTTPReply& reply) {
	if (!context.isAuthenticated) {
		LOG_DEBUG("Unauthorized access attempt to %s from %s", context.path.c_str(), context.clientIP.c_str());
		if (IsApiRequest(context)) {
			reply.status = eHTTPStatusCode::UNAUTHORIZED;
			reply.message = "{\"success\":false,\"error\":\"Authentication required\"}";
			reply.contentType = eContentType::APPLICATION_JSON;
		} else {
			reply.status = eHTTPStatusCode::FOUND;
			reply.message = "";
			reply.location = "/login";
			reply.contentType = eContentType::TEXT_HTML;
		}
		return false;
	}

	// Cookies are sent automatically by the browser, so state-changing requests authenticated by
	// cookie must also carry a header a cross-site form cannot set. Bearer-token clients are exempt.
	const auto authSource = context.userData.find("auth_source");
	if (!IsSafeMethod(context) && authSource != context.userData.end() && authSource->second == "cookie" &&
		context.GetHeader("X-Requested-With").empty()) {
		LOG("Rejected cookie-authenticated %s %s without X-Requested-With (possible CSRF) from %s",
			context.method.c_str(), context.path.c_str(), context.clientIP.c_str());
		reply.status = eHTTPStatusCode::FORBIDDEN;
		reply.message = "{\"success\":false,\"error\":\"Missing X-Requested-With header\"}";
		reply.contentType = eContentType::APPLICATION_JSON;
		return false;
	}

	// A token in the Authorization header is API use, which a GM level may not be allowed
	if (authSource != context.userData.end() && authSource->second == "header" && g_ApiAccessAllowed && !g_ApiAccessAllowed(context)) {
		reply.status = eHTTPStatusCode::FORBIDDEN;
		reply.message = "{\"success\":false,\"error\":\"API access isn't allowed for your account\"}";
		reply.contentType = eContentType::APPLICATION_JSON;
		return false;
	}

	if (context.userData.contains("needs_2fa") && !AllowedBeforeTwoFactor(context)) {
		if (IsApiRequest(context)) {
			reply.status = eHTTPStatusCode::FORBIDDEN;
			reply.message = "{\"success\":false,\"error\":\"Set up two-factor login on your account page first\",\"code\":\"2fa_required\"}";
			reply.contentType = eContentType::APPLICATION_JSON;
		} else {
			reply.status = eHTTPStatusCode::FOUND;
			reply.message = "";
			reply.location = "/accounts/" + std::to_string(context.accountId) + "#two-factor";
			reply.contentType = eContentType::TEXT_HTML;
		}
		return false;
	}

	const auto minGmLevel = requiredLevel();
	// A route guarded by a permission: the level, or a grant for this account (a deny takes it away)
	const bool allowed = permission.empty() ? context.gmLevel >= minGmLevel : Permissions::Allowed(context.gmLevel, permission, nullptr, context.grants.get());
	if (!allowed) {
		LOG_DEBUG("Forbidden access attempt by user %s (GM level %d < %d required) to %s from %s",
			context.authenticatedUser.c_str(), context.gmLevel, minGmLevel,
			context.path.c_str(), context.clientIP.c_str());
		// A page opened in the browser gets the dashboard's error page rather than raw JSON
		if (!IsApiRequest(context) && g_ForbiddenPage) {
			g_ForbiddenPage(context, reply);
			reply.status = eHTTPStatusCode::FORBIDDEN;
			return false;
		}
		reply.status = eHTTPStatusCode::FORBIDDEN;
		reply.message = "{\"success\":false,\"error\":\"Insufficient permissions\"}";
		reply.contentType = eContentType::APPLICATION_JSON;
		return false;
	}

	// An API key can only narrow what its owner may do: read-only keys make no changes, and a route guarded by a
	// permission needs that permission in the key's scope. Routes guarded only by a GM level above 0 have no
	// permission to scope them by, so only keys with all of the owner's permissions reach them.
	if (context.apiKey) {
		const auto& key = *context.apiKey;
		if (key.readOnly && !readsOnly && !IsSafeMethod(context)) {
			return RefuseApiKey(context, reply, context.method + " " + context.path + " with a read-only key", "This API key is read-only");
		}
		if (!permission.empty() && !key.Has(permission)) {
			return RefuseApiKey(context, reply, "no " + permission + " permission for " + context.path, "This API key doesn't have the " + permission + " permission");
		}
		if (permission.empty() && minGmLevel > 0 && !key.allPermissions) {
			return RefuseApiKey(context, reply, context.path + " needs a key with all permissions", "This needs an API key with all of your permissions");
		}
	}

	return true;
}
