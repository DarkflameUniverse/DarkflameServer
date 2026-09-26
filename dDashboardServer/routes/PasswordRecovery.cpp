#include "PasswordRecovery.h"

#include <ctime>

#include "RouteUtils.h"
#include "DashboardAuthService.h"
#include "EmailService.h"
#include "Totp.h"
#include "Alerts.h"
#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	// Emailed reset links (AccountRoutes.cpp): a new password makes any outstanding link useless
	constexpr const char* RESET_PURPOSE = "reset";
	constexpr size_t RECOVERY_CODE_LENGTH = 12;
	// Unknown usernames, accounts without two-factor login, wrong codes and accounts locked after wrong codes all get
	// this, so the form can't be used to find accounts or which of them have two-factor login
	constexpr const char* NO_MATCH = "That username and those codes don't match. After a few wrong tries, wait 15 minutes before trying again.";

	// Per client, like the emailed reset; each account also locks after a few wrong codes, like signing in
	RateLimiter g_Limiter(10, std::chrono::seconds(15 * 60));

	std::string ServerName() {
		const auto name = Game::config->GetValue("smtp_from_name");
		return name.empty() ? "DarkflameServer" : name;
	}
}

bool RecoveryResetEnabled() {
	return ConfigFlag("password_reset_recovery_codes", true);
}

void RegisterPasswordRecoveryRoutes() {
	Route(eHTTPMethod::POST, "/api/auth/recover_password", PUBLIC,
		"Set a new password with two-factor login instead of email. Body: {username, code (from the authenticator app), recovery_code, password}. "
		"Uses up the recovery code and signs the account out everywhere",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!RecoveryResetEnabled()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Password reset with recovery codes is not available on this server");
			if (!g_Limiter.Allow(ClientAddress(context))) return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many attempts, try again later");
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const std::string username = body->value("username", "");
			const std::string code = body->value("code", "");
			const std::string recoveryCode = body->value("recovery_code", "");
			const std::string password = body->value("password", "");
			// Before any code is used, so a typo in the new password doesn't cost a recovery code
			if (const auto error = ValidatePassword(password)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
			if (username.empty() || code.empty() || recoveryCode.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Fill in every field");

			const auto account = Database::Get()->GetAccountInfo(username);
			if (!account || Database::Get()->GetTotp(account->id).enabledAt == 0) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, NO_MATCH);
			const auto name = Database::Get()->GetAccountById(account->id).value("name", username);
			const auto address = ClientAddress(context);
			if (DashboardAuthService::IsThrottled(account->id, address)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, NO_MATCH);

			// The authenticator code first: getting it wrong costs nothing but waiting for the next one
			if (!DashboardAuthService::CheckTwoFactorCode(account->id, code, false)) {
				DashboardAuthService::RecordFailedAttempt(account->id, name, address);
				return JsonError(reply, eHTTPStatusCode::FORBIDDEN, NO_MATCH);
			}
			// Banned and locked accounts need staff; said only to someone who has the authenticator
			if (account->banned || account->locked) {
				return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "This account can't reset its password here. Contact a server administrator.");
			}
			const auto normalized = Totp::NormalizeRecoveryCode(recoveryCode);
			if (normalized.size() != RECOVERY_CODE_LENGTH || !Database::Get()->UseRecoveryCode(account->id, DashboardAuthService::Sha256Hex(normalized))) {
				DashboardAuthService::RecordFailedAttempt(account->id, name, address);
				return JsonError(reply, eHTTPStatusCode::FORBIDDEN, NO_MATCH);
			}

			Database::Get()->UpdateAccountPassword(account->id, HashPassword(password));
			Database::Get()->ClearFailedAttempts(account->id);
			Database::Get()->DeleteAccountTokens(account->id, RESET_PURPOSE);
			// Every dashboard session and API token issued before now stops working
			Database::Get()->SetSessionsValidAfter(account->id, std::time(nullptr));

			const auto left = Database::Get()->GetRecoveryCodesLeft(account->id);
			Database::Get()->InsertAuditLog(account->id, name, "password_reset",
				"With an authenticator code and a recovery code from " + address + " (" + std::to_string(left) + " recovery codes left)", account->id, 0);
			Alerts::Emit("security", "Password reset with a recovery code", name + " chose a new password with a two-factor recovery code from " + address + ".");
			if (const auto email = Database::Get()->GetAccountEmail(account->id); email && !email->email.empty() && EmailService::IsConfigured()) {
				EmailService::Send({ email->email, ServerName() + " password changed",
					"Hi " + name + ",\n\nThe password for your " + ServerName() + " account was just reset with one of your two-factor recovery codes. "
					"If this wasn't you, contact a server administrator immediately.\n" });
			}

			JsonSuccess(reply, {
				{"recoveryCodesLeft", left},
				{"message", "Your password has been changed and you were signed out everywhere. You have " + std::to_string(left) +
					" recovery code" + (left == 1 ? "" : "s") + " left; make new ones on your account page after you sign in."}
			});
		});
}
