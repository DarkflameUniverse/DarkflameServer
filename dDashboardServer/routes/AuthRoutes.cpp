#include "AuthRoutes.h"
#include "ApiKeyRoutes.h"
#include "Permissions.h"
#include "DashboardAuthService.h"
#include "AuthTokenHandler.h"
#include "JWTUtils.h"
#include "RequireAuthMiddleware.h"
#include "RouteUtils.h"
#include "WSRoutes.h"
#include "EmailService.h"
#include "AccountRoutes.h"
#include "eGameMasterLevel.h"
#include "json.hpp"
#include "Logger.h"
#include "Database.h"
#include "Web.h"
#include "eHTTPMethod.h"
#include "HTTPContext.h"
#include "Alerts.h"
#include "PasswordRecovery.h"

namespace {
	constexpr int64_t MAX_API_TOKEN_DAYS = 365;

	// Per-IP limits on unauthenticated endpoints, in addition to the per-account lockout
	RouteUtils::RateLimiter g_LoginLimiter(10, std::chrono::seconds(60));
	RouteUtils::RateLimiter g_RegisterLimiter(5, std::chrono::seconds(60 * 60));

	bool RegistrationEnabled() { return RouteUtils::ConfigFlag("allow_registration", false); }
	bool RegistrationRequiresPlayKey() { return RouteUtils::ConfigFlag("registration_requires_play_key", true); }
}

void RegisterAuthRoutes() {
	// POST /api/auth/login
	// Request body: { "username": "string", "password": "string", "rememberMe": boolean }
	// Sets an HttpOnly session cookie for the browser; the token is also returned for API clients.
	Game::web.RegisterHTTPRoute({
		.path = "/api/auth/login",
		.method = eHTTPMethod::POST,
		.middleware = {},
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			if (!g_LoginLimiter.Allow(RouteUtils::ClientAddress(context))) {
				return RouteUtils::JsonReply(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, { {"success", false}, {"message", "Too many login attempts, try again in a minute"} });
			}
			const auto json = RouteUtils::ParseBody(context);
			if (!json) return RouteUtils::JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");

			const std::string username = json->value("username", "");
			const std::string password = json->value("password", "");
			const bool rememberMe = json->value("rememberMe", false);

			if (username.empty() || password.empty()) {
				return RouteUtils::JsonReply(reply, eHTTPStatusCode::BAD_REQUEST, { {"success", false}, {"message", "Username and password are required"} });
			}

			const auto result = DashboardAuthService::Login(username, password, rememberMe, RouteUtils::ClientAddress(context));
			if (result.twoFactorRequired) {
				return RouteUtils::JsonReply(reply, eHTTPStatusCode::OK, {
					{"success", false}, {"twoFactorRequired", true}, {"challenge", result.challenge}, {"message", result.message}
				});
			}
			nlohmann::json response{ {"success", result.success}, {"message", result.message} };
			if (result.success) {
				response["token"] = result.token;
				response["gmLevel"] = result.gmLevel;
				reply.headers.push_back(AuthTokenHandler::BuildSessionCookie(result.token, rememberMe, RouteUtils::UseSecureCookies()));
				const auto info = Database::Get()->GetAccountInfo(username);
				Database::Get()->InsertAuditLog(info ? info->id : 0, username, "dashboard_login", "Login from " + RouteUtils::ClientAddress(context), info ? info->id : 0, 0);
			}
			RouteUtils::JsonReply(reply, result.success ? eHTTPStatusCode::OK : eHTTPStatusCode::UNAUTHORIZED, response);
		}
	});

	// POST /api/auth/login/2fa - Second step for accounts with two-factor login
	// Request body: { "challenge": "from /api/auth/login", "code": "123456 or a recovery code" }
	Game::web.RegisterHTTPRoute({
		.path = "/api/auth/login/2fa",
		.method = eHTTPMethod::POST,
		.middleware = {},
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			if (!g_LoginLimiter.Allow(RouteUtils::ClientAddress(context))) {
				return RouteUtils::JsonReply(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, { {"success", false}, {"message", "Too many login attempts, try again in a minute"} });
			}
			const auto json = RouteUtils::ParseBody(context);
			if (!json) return RouteUtils::JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");

			const auto result = DashboardAuthService::CompleteTwoFactor(json->value("challenge", ""), json->value("code", ""));
			nlohmann::json response{ {"success", result.success}, {"message", result.message} };
			if (result.success) {
				response["token"] = result.token;
				response["gmLevel"] = result.gmLevel;
				// The cookie lifetime follows the token's (remember me was chosen in the first step)
				JWTUtils::JWTPayload payload;
				const bool longLived = JWTUtils::ValidateToken(result.token, payload) && payload.rememberMe;
				reply.headers.push_back(AuthTokenHandler::BuildSessionCookie(result.token, longLived, RouteUtils::UseSecureCookies()));
				Database::Get()->InsertAuditLog(result.accountId, payload.username, "dashboard_login",
					"Login with two-factor " + std::string(result.usedRecoveryCode ? "recovery code" : "code") + " from " + RouteUtils::ClientAddress(context), result.accountId, 0);
				if (result.usedRecoveryCode) {
					Alerts::Emit("security", "Recovery code used", payload.username + " signed in with a two-factor recovery code from " + RouteUtils::ClientAddress(context) + ".");
				}
			}
			RouteUtils::JsonReply(reply, result.success ? eHTTPStatusCode::OK : eHTTPStatusCode::UNAUTHORIZED, response);
		}
	});

	// GET /api/auth/config - Public settings the login page needs
	RouteUtils::Route(eHTTPMethod::GET, "/api/auth/config", RouteUtils::PUBLIC, "Whether self-registration is enabled",
		[](HTTPReply& reply, const HTTPContext& context) {
			RouteUtils::JsonReply(reply, eHTTPStatusCode::OK, {
				{"registration", RegistrationEnabled()},
				{"playKeyRequired", RegistrationRequiresPlayKey()},
				{"emailEnabled", EmailService::IsConfigured()},
				{"emailRequired", EmailService::IsConfigured() && RouteUtils::ConfigFlag("registration_requires_email", false)},
				{"recoveryReset", RecoveryResetEnabled()},
				// Pages open without signing in (PublicRoutes.cpp, Showcase.cpp)
				{"publicStatus", RouteUtils::ConfigFlag("public_status", false)},
				{"publicShowcase", RouteUtils::ConfigFlag("showcase_public", false)}
			});
		});

	// POST /api/auth/register - Self-registration (allow_registration=1)
	RouteUtils::Route(eHTTPMethod::POST, "/api/auth/register", RouteUtils::PUBLIC, "Create an account. Body: {username, password, play_key, email}",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!RegistrationEnabled()) return RouteUtils::JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Registration is disabled");
			if (!g_RegisterLimiter.Allow(RouteUtils::ClientAddress(context))) {
				return RouteUtils::JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many registrations from your address, try again later");
			}

			const auto json = RouteUtils::ParseBody(context);
			if (!json) return RouteUtils::JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const std::string username = json->value("username", "");
			const std::string password = json->value("password", "");
			const std::string playKey = json->value("play_key", "");
			const std::string email = json->value("email", "");
			const bool emailRequired = EmailService::IsConfigured() && RouteUtils::ConfigFlag("registration_requires_email", false);
			if (emailRequired && email.empty()) return RouteUtils::JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "An email address is required");
			if (!email.empty() && !Smtp::IsValidAddress(email)) return RouteUtils::JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "That is not a valid email address");
			if (!email.empty() && Database::Get()->GetAccountIdByConfirmedEmail(email)) return RouteUtils::JsonError(reply, eHTTPStatusCode::CONFLICT, "That email address is already used by another account");

			if (const auto error = RouteUtils::ValidateUsername(username)) return RouteUtils::JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
			if (const auto error = RouteUtils::ValidatePassword(password)) return RouteUtils::JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);

			std::optional<int32_t> keyId;
			if (RegistrationRequiresPlayKey() || !playKey.empty()) {
				keyId = Database::Get()->GetRedeemablePlayKeyId(playKey);
				if (!keyId) return RouteUtils::JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "That play key is invalid, inactive or used up");
			}
			if (Database::Get()->GetAccountInfo(username)) return RouteUtils::JsonError(reply, eHTTPStatusCode::CONFLICT, "That username is taken");

			Database::Get()->InsertNewAccount(username, RouteUtils::HashPassword(password), eGameMasterLevel::CIVILIAN);
			const auto account = Database::Get()->GetAccountInfo(username);
			if (!account) return RouteUtils::JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Account creation failed");
			if (keyId) Database::Get()->SetAccountPlayKey(account->id, *keyId);

			Database::Get()->InsertAuditLog(account->id, username, "register_account",
				"Registered from " + RouteUtils::ClientAddress(context) + (keyId ? " with play key ID " + std::to_string(*keyId) : ""), account->id, 0);
			BroadcastTableChanged("accounts");
			BroadcastTableChanged("play_keys");

			std::string message = "Account created. You can now log in to the game and the dashboard.";
			if (!email.empty() && EmailService::IsConfigured()) {
				// Sent through the same path as a user changing their address
				Database::Get()->SetAccountEmail(account->id, email, false);
				SendVerificationEmail(account->id, username, email, 0);
				message += " Check " + email + " to confirm your email address.";
			}
			RouteUtils::JsonSuccess(reply, { {"message", message} });
		});

	// POST /api/auth/logout - Clear the session cookie
	Game::web.RegisterHTTPRoute({
		.path = "/api/auth/logout",
		.method = eHTTPMethod::POST,
		.middleware = {},
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			reply.headers.push_back(AuthTokenHandler::BuildClearSessionCookie(RouteUtils::UseSecureCookies()));
			RouteUtils::JsonReply(reply, eHTTPStatusCode::OK, { {"success", true} });
		}
	});

	// GET /api/auth/me - Current session info
	Game::web.RegisterHTTPRoute({
		.path = "/api/auth/me",
		.method = eHTTPMethod::GET,
		.middleware = {},
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			if (!context.isAuthenticated) return RouteUtils::JsonReply(reply, eHTTPStatusCode::OK, { {"valid", false} });
			RouteUtils::JsonReply(reply, eHTTPStatusCode::OK, {
				{"valid", true},
				{"username", context.authenticatedUser},
				{"accountId", context.accountId},
				{"gmLevel", context.gmLevel}
			});
		}
	});

	// POST /api/auth/verify
	// Request body: { "token": "string" }
	// Response: { "valid": boolean, "username": "string", "gmLevel": number }
	Game::web.RegisterHTTPRoute({
		.path = "/api/auth/verify",
		.method = eHTTPMethod::POST,
		.middleware = {},
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			const auto json = RouteUtils::ParseBody(context);
			const std::string token = json ? json->value("token", "") : "";
			const auto result = AuthTokenHandler::ValidateToken(token);

			nlohmann::json response{ {"valid", result.isValid} };
			if (result.isValid) {
				response["username"] = result.username;
				response["gmLevel"] = result.gmLevel;
			}
			RouteUtils::JsonReply(reply, eHTTPStatusCode::OK, response);
		}
	});

	// POST /api/auth/token - Issue a bearer token for API clients (bots, scripts)
	// Request body: { "days": number (1-365, default 30) }
	// Kept for scripts written for the old API tokens: it now makes an API key named "API token" with all of the
	// caller's permissions (which always follow the account's current GM level). Pick a narrower scope, limits and
	// no expiry with POST /api/api_keys. Tokens made before API keys keep working until they expire.
	Game::web.RegisterHTTPRoute({
		.path = "/api/auth/token",
		.method = eHTTPMethod::POST,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			if (!Permissions::Allowed(context.gmLevel, "api_access", nullptr, context.grants.get())) return RouteUtils::JsonError(reply, eHTTPStatusCode::FORBIDDEN, "API access isn't allowed for your account");
			// Only a signed-in browser session may make tokens: a leaked token must not be able to renew itself for a year
			const auto source = context.userData.find("auth_source");
			if (source == context.userData.end() || source->second != "cookie") {
				return RouteUtils::JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Make API tokens from your account page while signed in, not with another token");
			}
			const auto json = RouteUtils::ParseBody(context);
			const int64_t days = std::clamp<int64_t>(json ? json->value("days", 30) : 30, 1, MAX_API_TOKEN_DAYS);
			const auto token = ApiKeyRoutes::CreateFullKey(context, "API token", days);
			RouteUtils::JsonReply(reply, eHTTPStatusCode::OK, { {"token", token}, {"expiresInDays", days} });
		}
	});
}
