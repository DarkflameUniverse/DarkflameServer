#include "SecurityRoutes.h"

#include <chrono>
#include <ctime>
#include <map>

#include <bcrypt/BCrypt.hpp>

#include "RouteUtils.h"
#include "AuthTokenHandler.h"
#include "DashboardAuthService.h"
#include "Database.h"
#include "JWTUtils.h"
#include "Totp.h"
#include "WSRoutes.h"
#include "eHTTPMethod.h"
#include "Game.h"
#include "dConfig.h"

using namespace RouteUtils;

namespace {
	constexpr int64_t SETUP_LIFETIME = 10 * 60;
	constexpr size_t RECOVERY_CODES = 10;

	// Secrets being set up but not yet confirmed with a code, by account
	struct PendingSetup {
		std::string secret;
		int64_t expires{};
	};
	std::map<uint32_t, PendingSetup> g_PendingSetups;

	RateLimiter g_CodeLimiter(10, std::chrono::seconds(60));

	std::string Issuer() {
		const auto configured = Game::config ? Game::config->GetValue("totp_issuer") : "";
		return configured.empty() ? "DarkflameServer" : configured;
	}

	// New recovery codes for an account; returns them in plain text to show once
	std::vector<std::string> NewRecoveryCodes(uint32_t accountId) {
		const auto codes = Totp::GenerateRecoveryCodes(RECOVERY_CODES);
		std::vector<std::string> hashes;
		for (const auto& code : codes) hashes.push_back(DashboardAuthService::Sha256Hex(Totp::NormalizeRecoveryCode(code)));
		Database::Get()->ReplaceRecoveryCodes(accountId, hashes);
		return codes;
	}

	// After turning two-factor login on or off, sign out every other session and keep this one
	void RefreshSession(HTTPReply& reply, const HTTPContext& context) {
		Database::Get()->SetSessionsValidAfter(context.accountId, std::time(nullptr));
		const auto token = JWTUtils::GenerateSessionToken(context.accountId, context.authenticatedUser, context.gmLevel, false);
		if (!token.empty()) reply.headers.push_back(AuthTokenHandler::BuildSessionCookie(token, false, UseSecureCookies()));
	}

	bool CheckPassword(const HTTPContext& context, const std::string& password) {
		const auto info = Database::Get()->GetAccountInfo(context.authenticatedUser);
		return info && !password.empty() && password.size() <= 40 && ::bcrypt_checkpw(password.c_str(), info->bcryptPassword.c_str()) == 0;
	}
}

void RegisterSecurityRoutes() {
	Route(eHTTPMethod::GET, "/api/account/2fa", 0, "Your two-factor login status",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto totp = Database::Get()->GetTotp(context.accountId);
			const auto required = DashboardAuthService::RequiredTwoFactorLevel();
			JsonReply(reply, eHTTPStatusCode::OK, {
				{"enabled", totp.enabledAt != 0}, {"enabledAt", totp.enabledAt},
				{"recoveryCodesLeft", totp.enabledAt != 0 ? Database::Get()->GetRecoveryCodesLeft(context.accountId) : 0},
				{"required", required > 0 && context.gmLevel >= required}
			});
		});

	Route(eHTTPMethod::POST, "/api/account/2fa/setup", 0, "Start setting up two-factor login: returns a secret and an otpauth:// link for an authenticator app",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (Database::Get()->GetTotp(context.accountId).enabledAt != 0) return JsonError(reply, eHTTPStatusCode::CONFLICT, "Two-factor login is already on");
			const auto secret = Totp::GenerateSecret();
			if (secret.empty()) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Could not create a secret");
			g_PendingSetups[context.accountId] = { secret, static_cast<int64_t>(std::time(nullptr)) + SETUP_LIFETIME };
			JsonSuccess(reply, { {"secret", secret}, {"uri", Totp::ProvisioningUri(Issuer(), context.authenticatedUser, secret)} });
		});

	Route(eHTTPMethod::POST, "/api/account/2fa/enable", 0, "Finish setting up two-factor login. Body: {code}. Returns recovery codes, shown once",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!g_CodeLimiter.Allow(std::to_string(context.accountId))) return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many attempts, wait a minute");
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto it = g_PendingSetups.find(context.accountId);
			if (it == g_PendingSetups.end() || it->second.expires < std::time(nullptr)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Setup expired, start again");

			const auto step = Totp::Verify(it->second.secret, body->value("code", ""), std::time(nullptr));
			if (!step) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "That code is not right. Check the time on your phone and try the next code.");
			const auto encrypted = Totp::EncryptSecret(it->second.secret);
			if (!encrypted) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Two-factor login is unavailable on this server (no key)");

			Database::Get()->SetTotp(context.accountId, *encrypted, std::time(nullptr));
			Database::Get()->UseTotpStep(context.accountId, *step);
			g_PendingSetups.erase(it);
			const auto codes = NewRecoveryCodes(context.accountId);
			RefreshSession(reply, context);
			Audit(context, "enable_2fa", context.authenticatedUser + " turned on two-factor login");
			BroadcastTableChanged("accounts", std::to_string(context.accountId));
			JsonSuccess(reply, { {"recoveryCodes", codes}, {"message", "Two-factor login is on. Other sessions were signed out."} });
		});

	Route(eHTTPMethod::POST, "/api/account/2fa/disable", 0, "Turn off two-factor login. Body: {password, code}",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!g_CodeLimiter.Allow(std::to_string(context.accountId))) return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many attempts, wait a minute");
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto required = DashboardAuthService::RequiredTwoFactorLevel();
			if (required > 0 && context.gmLevel >= required) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Two-factor login is required for your GM level");
			if (!CheckPassword(context, body->value("password", ""))) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Password is incorrect");
			if (!DashboardAuthService::CheckTwoFactorCode(context.accountId, body->value("code", ""), true)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "That code is not right");

			Database::Get()->SetTotp(context.accountId, "", 0);
			Database::Get()->ReplaceRecoveryCodes(context.accountId, {});
			RefreshSession(reply, context);
			Audit(context, "disable_2fa", context.authenticatedUser + " turned off two-factor login");
			BroadcastTableChanged("accounts", std::to_string(context.accountId));
			JsonSuccess(reply, { {"message", "Two-factor login is off"} });
		});

	Route(eHTTPMethod::POST, "/api/account/2fa/recovery_codes", 0, "Replace your recovery codes. Body: {code}. Returns the new codes, shown once",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!g_CodeLimiter.Allow(std::to_string(context.accountId))) return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many attempts, wait a minute");
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			if (!DashboardAuthService::CheckTwoFactorCode(context.accountId, body->value("code", ""), false)) {
				return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Enter a current code from your authenticator app");
			}
			const auto codes = NewRecoveryCodes(context.accountId);
			Audit(context, "regenerate_recovery_codes", context.authenticatedUser + " made new recovery codes");
			JsonSuccess(reply, { {"recoveryCodes", codes} });
		});

	Route(eHTTPMethod::POST, "/api/accounts/:id/2fa/reset", Perm("accounts_manage"), "Turn off another account's two-factor login (for a lost phone) and sign it out everywhere",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto accountId = PathId<uint32_t>(context.path, 2);
			if (!accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION)) return;
			Database::Get()->SetTotp(*accountId, "", 0);
			Database::Get()->ReplaceRecoveryCodes(*accountId, {});
			Database::Get()->SetSessionsValidAfter(*accountId, std::time(nullptr));
			const auto name = Database::Get()->GetAccountById(*accountId).value("name", std::string{});
			Audit(context, "reset_2fa", "Turned off two-factor login for " + name + " (account ID " + std::to_string(*accountId) + ")", AuditTarget::Account(*accountId));
			BroadcastTableChanged("accounts", std::to_string(*accountId));
			JsonSuccess(reply, { {"message", "Two-factor login turned off for " + name + ". They were signed out everywhere."} });
		});

	Route(eHTTPMethod::POST, "/api/account/sessions/revoke", 0, "Sign out every other session and API token of your account (this one stays signed in)",
		[](HTTPReply& reply, const HTTPContext& context) {
			RefreshSession(reply, context);
			Audit(context, "revoke_sessions", context.authenticatedUser + " signed out their other sessions and API tokens");
			JsonSuccess(reply, { {"message", "Every other session and API token is signed out"} });
		});

	Route(eHTTPMethod::POST, "/api/accounts/:id/sessions/revoke", Perm("accounts_manage"), "Sign an account out of every dashboard session and API token",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto accountId = PathId<uint32_t>(context.path, 2);
			if (!accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::TOOLS)) return;
			Database::Get()->SetSessionsValidAfter(*accountId, std::time(nullptr));
			const auto name = Database::Get()->GetAccountById(*accountId).value("name", std::string{});
			Audit(context, "revoke_sessions", "Signed " + name + " (account ID " + std::to_string(*accountId) + ") out of every session and API token", AuditTarget::Account(*accountId));
			JsonSuccess(reply, { {"message", name + " is signed out everywhere"} });
		});
}
