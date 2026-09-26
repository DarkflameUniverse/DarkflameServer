#include "AccountRoutes.h"

#include "RouteUtils.h"
#include "PlayerActions.h"
#include "EmailService.h"
#include "AuthTokenHandler.h"
#include "JWTUtils.h"
#include "WSRoutes.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "eHTTPMethod.h"
#include "eGameMasterLevel.h"
#include <bcrypt/BCrypt.hpp>
#include <ctime>
#include <map>
#include <regex>
#include "OAuth2.h"

using namespace RouteUtils;

namespace {
	constexpr const char* RESET_PURPOSE = "reset";
	constexpr const char* VERIFY_PURPOSE = "verify";
	constexpr int64_t RESET_LIFETIME = 60 * 60;          // 1 hour
	constexpr int64_t VERIFY_LIFETIME = 24 * 60 * 60;    // 1 day
	constexpr const char* GENERIC_FORGOT_REPLY = "If that account has a verified email address, a reset link is on its way.";

	// Unauthenticated email endpoints are limited per client and per account
	RateLimiter g_ForgotLimiter(5, std::chrono::seconds(15 * 60));
	RateLimiter g_ResetLimiter(10, std::chrono::seconds(15 * 60));
	RateLimiter g_PerAccountEmailLimiter(3, std::chrono::seconds(60 * 60));

	std::string ServerName() {
		const auto name = Game::config->GetValue("smtp_from_name");
		return name.empty() ? "DarkflameServer" : name;
	}

	// Queue an email and report its outcome under a request id the dashboard UI can wait on
	// Only requester (who asked for the email) sees the outcome
	uint32_t SendTracked(Smtp::Message message, const std::string& successMessage, uint32_t requester) {
		const auto requestId = PlayerActions::Begin(requester);
		EmailService::Send(std::move(message), [requestId, successMessage](const std::optional<std::string>& error) {
			PlayerActions::Finish(requestId, error ? PlayerActions::Outcome{ false, "Email failed: " + *error } : PlayerActions::Outcome{ true, successMessage });
		});
		return requestId;
	}

	uint32_t SendPasswordReset(uint32_t accountId, const std::string& username, const std::string& email, uint32_t requester) {
		Database::Get()->DeleteExpiredAccountTokens();
		Database::Get()->DeleteAccountTokens(accountId, RESET_PURPOSE);
		const auto token = GenerateUrlToken();
		Database::Get()->InsertAccountToken(HashToken(token), accountId, RESET_PURPOSE, "", std::time(nullptr) + RESET_LIFETIME);

		Smtp::Message message;
		message.to = email;
		message.subject = ServerName() + " password reset";
		message.body =
			"Hi " + username + ",\n\n"
			"Someone asked to reset the password for your " + ServerName() + " account. "
			"If that was you, choose a new password here within the next hour:\n\n" +
			EmailService::BaseUrl() + "/reset_password?token=" + token + "\n\n"
			"If you didn't ask for this, you can ignore this email; your password won't change.\n";
		return SendTracked(std::move(message), "Password reset email sent to the account's confirmed address", requester);
	}

	uint32_t SendVerification(uint32_t accountId, const std::string& username, const std::string& email, uint32_t requester) {
		return SendVerificationEmail(accountId, username, email, requester);
	}

	void SendNotice(const std::string& email, const std::string& subject, const std::string& body) {
		if (email.empty() || !EmailService::IsConfigured()) return;
		EmailService::Send({ email, subject, body });
	}

	// Validate and store a new address (unconfirmed) and send the verification link
	std::optional<std::string> ChangeEmail(uint32_t accountId, const std::string& username, const std::string& email, uint32_t requester, uint32_t& requestId) {
		if (email.empty()) {
			Database::Get()->SetAccountEmail(accountId, "", false);
			Database::Get()->DeleteAccountTokens(accountId, VERIFY_PURPOSE);
			return std::nullopt;
		}
		if (!Smtp::IsValidAddress(email)) return "That is not a valid email address";
		const auto owner = Database::Get()->GetAccountIdByConfirmedEmail(email);
		if (owner && *owner != accountId) return "That email address is already used by another account";
		if (!EmailService::IsConfigured()) return "Email is not configured on this server";

		Database::Get()->SetAccountEmail(accountId, email, false);
		requestId = SendVerification(accountId, username, email, requester);
		return std::nullopt;
	}

	// Pending "connect mail account" flows: state -> who started it and the PKCE verifier
	struct PendingOAuth2 {
		uint32_t accountId{};
		std::string codeVerifier;
		int64_t expiresAt{};
	};
	constexpr int64_t OAUTH2_STATE_LIFETIME = 10 * 60;
	std::map<std::string, PendingOAuth2> g_PendingOAuth2;

	std::string AccountName(uint32_t accountId) {
		return Database::Get()->GetAccountById(accountId).value("name", std::string{});
	}
}

uint32_t SendVerificationEmail(uint32_t accountId, const std::string& username, const std::string& email, uint32_t requester) {
	Database::Get()->DeleteExpiredAccountTokens();
	Database::Get()->DeleteAccountTokens(accountId, VERIFY_PURPOSE);
	const auto token = GenerateUrlToken();
	// The address is stored with the token so a later change of address invalidates this link
	Database::Get()->InsertAccountToken(HashToken(token), accountId, VERIFY_PURPOSE, email, std::time(nullptr) + VERIFY_LIFETIME);

	Smtp::Message message;
	message.to = email;
	message.subject = "Confirm your email for " + ServerName();
	message.body =
		"Hi " + username + ",\n\n"
		"Please confirm this email address for your " + ServerName() + " account so you can reset your password if you ever forget it:\n\n" +
		EmailService::BaseUrl() + "/verify_email?token=" + token + "\n\n"
		"This link expires in 24 hours. If you didn't expect this email, you can ignore it.\n";
	return SendTracked(std::move(message), "Verification email sent", requester);
}

void RegisterAccountRoutes() {
	// ---- Public: password reset and email verification ----

	Route(eHTTPMethod::POST, "/api/auth/forgot_password", PUBLIC, "Email a password reset link. Body: {identifier: username or email}",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!EmailService::IsConfigured()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Password reset by email is not available on this server");
			if (!g_ForgotLimiter.Allow(ClientAddress(context))) return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many requests, try again later");
			const auto body = ParseBody(context);
			const std::string identifier = body ? body->value("identifier", "") : "";

			// The reply is the same whether or not the account exists, so this can't be used to find accounts
			std::optional<uint32_t> accountId;
			if (identifier.find('@') != std::string::npos) accountId = Database::Get()->GetAccountIdByConfirmedEmail(identifier);
			else if (const auto info = Database::Get()->GetAccountInfo(identifier)) accountId = info->id;

			if (accountId && g_PerAccountEmailLimiter.Allow("reset:" + std::to_string(*accountId))) {
				const auto email = Database::Get()->GetAccountEmail(*accountId);
				const auto account = Database::Get()->GetAccountById(*accountId);
				if (email && email->confirmed && !email->email.empty() && !account.value("banned", 0)) {
					SendPasswordReset(*accountId, account.value("name", std::string{}), email->email, 0);
					Database::Get()->InsertAuditLog(*accountId, account.value("name", std::string{}), "password_reset_requested", "From " + ClientAddress(context), *accountId, 0);
				}
			}
			JsonSuccess(reply, { {"message", GENERIC_FORGOT_REPLY} });
		});

	Route(eHTTPMethod::POST, "/api/auth/reset_password", PUBLIC, "Set a new password with an emailed token. Body: {token, password}",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!g_ResetLimiter.Allow(ClientAddress(context))) return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many attempts, try again later");
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const std::string token = body->value("token", "");
			const std::string password = body->value("password", "");
			// Check the password before consuming the token so a typo doesn't burn the link
			if (const auto error = ValidatePassword(password)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);

			const auto consumed = token.empty() ? std::nullopt : Database::Get()->ConsumeAccountToken(HashToken(token), RESET_PURPOSE);
			if (!consumed) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "This reset link is invalid or has expired. Request a new one.");

			const auto accountId = consumed->accountId;
			Database::Get()->UpdateAccountPassword(accountId, HashPassword(password));
			Database::Get()->DeleteAccountTokens(accountId, RESET_PURPOSE);
			Database::Get()->ClearFailedAttempts(accountId);
			// Sign out every existing session
			Database::Get()->SetSessionsValidAfter(accountId, std::time(nullptr));

			const auto name = AccountName(accountId);
			Database::Get()->InsertAuditLog(accountId, name, "password_reset", "Via emailed link from " + ClientAddress(context), accountId, 0);
			if (const auto email = Database::Get()->GetAccountEmail(accountId)) {
				SendNotice(email->email, ServerName() + " password changed",
					"Hi " + name + ",\n\nThe password for your " + ServerName() + " account was just reset. "
					"If this wasn't you, contact a server administrator immediately.\n");
			}
			JsonSuccess(reply, { {"message", "Your password has been changed. You can now log in."} });
		});

	Route(eHTTPMethod::POST, "/api/auth/verify_email", PUBLIC, "Confirm an email address with an emailed token. Body: {token}",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!g_ResetLimiter.Allow(ClientAddress(context))) return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many attempts, try again later");
			const auto body = ParseBody(context);
			const std::string token = body ? body->value("token", "") : "";
			const auto consumed = token.empty() ? std::nullopt : Database::Get()->ConsumeAccountToken(HashToken(token), VERIFY_PURPOSE);
			if (!consumed) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "This confirmation link is invalid or has expired.");

			const auto current = Database::Get()->GetAccountEmail(consumed->accountId);
			if (!current || current->email != consumed->data) {
				return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "This link is for an email address that is no longer on the account.");
			}
			const auto owner = Database::Get()->GetAccountIdByConfirmedEmail(consumed->data);
			if (owner && *owner != consumed->accountId) return JsonError(reply, eHTTPStatusCode::CONFLICT, "That email address is already used by another account");

			Database::Get()->SetAccountEmail(consumed->accountId, consumed->data, true);
			Database::Get()->InsertAuditLog(consumed->accountId, AccountName(consumed->accountId), "email_verified", consumed->data, consumed->accountId, 0);
			BroadcastTableChanged("accounts", std::to_string(consumed->accountId));
			JsonSuccess(reply, { {"message", "Your email address is confirmed."} });
		});

	// ---- Your own account ----

	Route(eHTTPMethod::GET, "/api/account/preferences", 0, "Your saved view choices on the dashboard's pages (show staff, the 3D viewer's toggles, ...)",
		[](HTTPReply& reply, const HTTPContext& context) {
			JsonSuccess(reply, { {"preferences", nlohmann::json::parse(Database::Get()->GetDashboardPreferences(context.accountId), nullptr, false)} });
		});

	Route(eHTTPMethod::POST, "/api/account/preferences", 0,
		"Save view choices; they're merged into what's saved. Body: {name: value, ...} (a name is up to 64 letters, digits, . - _; "
		"a value is true/false, a number or text up to 200 characters; null forgets it)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Send an object of names and values");
			static const std::regex namePattern("^[A-Za-z0-9._-]{1,64}$");
			for (const auto& [name, value] : body->items()) {
				if (!std::regex_match(name, namePattern)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Bad preference name: " + name);
				const bool fits = value.is_null() || value.is_boolean() || value.is_number() || (value.is_string() && value.get<std::string>().size() <= 200);
				if (!fits) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "A preference is true/false, a number or short text");
			}
			auto saved = nlohmann::json::parse(Database::Get()->GetDashboardPreferences(context.accountId), nullptr, false);
			if (!saved.is_object()) saved = nlohmann::json::object();
			saved.merge_patch(*body); // null removes
			if (saved.size() > 200) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Too many saved preferences");
			Database::Get()->SetDashboardPreferences(context.accountId, saved.dump());
			JsonSuccess(reply, { {"preferences", saved} });
		});

	Route(eHTTPMethod::GET, "/api/account/email", 0, "Your email address and whether it is confirmed",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto email = Database::Get()->GetAccountEmail(context.accountId);
			JsonReply(reply, eHTTPStatusCode::OK, {
				{"email", email ? email->email : ""},
				{"confirmed", email && email->confirmed},
				{"emailEnabled", EmailService::IsConfigured()}
			});
		});

	Route(eHTTPMethod::POST, "/api/account/email", 0, "Set your email address (sends a confirmation link). Body: {email, current_password}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const std::string email = body->value("email", "");
			const std::string current = body->value("current_password", "");

			// Re-authenticate: otherwise a stolen session could redirect password resets to an attacker's address
			const auto info = Database::Get()->GetAccountInfo(context.authenticatedUser);
			if (!info || current.size() > MAX_PASSWORD_LENGTH || ::bcrypt_checkpw(current.c_str(), info->bcryptPassword.c_str()) != 0) {
				return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Current password is incorrect");
			}
			if (!email.empty() && !g_PerAccountEmailLimiter.Allow("verify:" + std::to_string(context.accountId))) {
				return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many emails requested, try again later");
			}

			const auto previous = Database::Get()->GetAccountEmail(context.accountId);
			uint32_t requestId = 0;
			if (const auto error = ChangeEmail(context.accountId, context.authenticatedUser, email, context.accountId, requestId)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);

			if (previous && previous->confirmed && previous->email != email) {
				SendNotice(previous->email, ServerName() + " email address changed",
					"Hi " + context.authenticatedUser + ",\n\nThe email address on your " + ServerName() + " account was changed" +
					(email.empty() ? " (removed)" : " to " + email) + ". If this wasn't you, contact a server administrator immediately.\n");
			}
			Audit(context, "change_own_email", email.empty() ? "Removed" : email);
			nlohmann::json response{ {"message", email.empty() ? "Email address removed" : "Check " + email + " for a confirmation link"} };
			if (requestId) response["requestId"] = requestId;
			JsonSuccess(reply, response);
		});

	Route(eHTTPMethod::POST, "/api/account/email/resend", 0, "Resend the confirmation link for your email address",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto email = Database::Get()->GetAccountEmail(context.accountId);
			if (!email || email->email.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "You have no email address set");
			if (email->confirmed) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Your email address is already confirmed");
			if (!EmailService::IsConfigured()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Email is not configured on this server");
			if (!g_PerAccountEmailLimiter.Allow("verify:" + std::to_string(context.accountId))) {
				return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many emails requested, try again later");
			}
			JsonSuccess(reply, { {"requestId", SendVerification(context.accountId, context.authenticatedUser, email->email, context.accountId)} });
		});

	// ---- Administration ----

	Route(eHTTPMethod::POST, "/api/accounts/:id/email", Perm("accounts_manage"), "Set an account's email address (sends a confirmation link). Body: {email}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto accountId = PathId<uint32_t>(context.path, 2);
			const auto body = ParseBody(context);
			if (!accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION)) return;

			const std::string email = body->value("email", "");
			uint32_t requestId = 0;
			if (const auto error = ChangeEmail(*accountId, AccountName(*accountId), email, context.accountId, requestId)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
			Audit(context, "set_account_email", "Account ID " + std::to_string(*accountId) + ": " + (email.empty() ? "removed" : email), AuditTarget::Account(*accountId));
			BroadcastTableChanged("accounts", std::to_string(*accountId));
			nlohmann::json response{ {"message", email.empty() ? "Email address removed" : "Confirmation link sent to " + email} };
			if (requestId) response["requestId"] = requestId;
			JsonSuccess(reply, response);
		});

	Route(eHTTPMethod::POST, "/api/accounts/:id/send_password_reset", Perm("accounts_send_reset"), "Email the account a password reset link (requires a confirmed address)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto accountId = PathId<uint32_t>(context.path, 2);
			if (!accountId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::TOOLS)) return;
			if (!EmailService::IsConfigured()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Email is not configured on this server");

			const auto email = Database::Get()->GetAccountEmail(*accountId);
			if (!email || email->email.empty() || !email->confirmed) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "This account has no confirmed email address");
			Audit(context, "send_password_reset", "Account ID " + std::to_string(*accountId), AuditTarget::Account(*accountId));
			JsonSuccess(reply, { {"requestId", SendPasswordReset(*accountId, AccountName(*accountId), email->email, context.accountId)} });
		});

	Route(eHTTPMethod::GET, "/api/email/status", Perm("email_settings"), "Email settings status, including the connected OAuth2 account",
		[](HTTPReply& reply, const HTTPContext& context) {
			JsonReply(reply, eHTTPStatusCode::OK, EmailService::OAuth2Status());
		});

	Route(eHTTPMethod::POST, "/api/email/oauth2/start", Perm("email_settings"), "Begin connecting the mail account; returns the provider sign-in URL",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto now = static_cast<int64_t>(std::time(nullptr));
			std::erase_if(g_PendingOAuth2, [now](const auto& entry) { return entry.second.expiresAt <= now; });

			const auto state = GenerateUrlToken();
			const auto verifier = GenerateUrlToken(); // 64 hex characters, within PKCE's 43-128
			const auto url = EmailService::OAuth2AuthorizeUrl(state, OAuth2::PkceChallenge(verifier));
			if (url.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "OAuth2 sign-in is not set up (smtp_auth=oauth2 with the authorization_code grant)");

			g_PendingOAuth2[state] = { context.accountId, verifier, now + OAUTH2_STATE_LIFETIME };
			JsonSuccess(reply, { {"url", url} });
		});

	Route(eHTTPMethod::POST, "/api/email/oauth2/complete", Perm("email_settings"), "Finish connecting the mail account. Body: {code, state}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const std::string state = body->value("state", "");
			const std::string code = body->value("code", "");

			// The state must come from a flow this operator started, which stops another site from
			// completing the flow with its own code (login CSRF)
			const auto it = g_PendingOAuth2.find(state);
			if (it == g_PendingOAuth2.end() || it->second.accountId != context.accountId || it->second.expiresAt <= static_cast<int64_t>(std::time(nullptr))) {
				return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "This sign-in has expired or was not started by you. Start again from your account page.");
			}
			const auto verifier = it->second.codeVerifier;
			g_PendingOAuth2.erase(it);
			if (code.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The provider did not return an authorization code");

			const auto requestId = PlayerActions::Begin(context.accountId);
			const auto actor = context.authenticatedUser;
			const auto actorId = context.accountId;
			EmailService::OAuth2Connect(code, verifier, [requestId, actor, actorId](const std::optional<std::string>& error) {
				if (!error) Database::Get()->InsertAuditLog(actorId, actor, "connect_mail_account", "OAuth2 mail account connected", 0, 0);
				PlayerActions::Finish(requestId, error ? PlayerActions::Outcome{ false, "Connecting failed: " + *error } : PlayerActions::Outcome{ true, "Mail account connected" });
			});
			JsonSuccess(reply, { {"requestId", requestId} });
		});

	Route(eHTTPMethod::POST, "/api/email/oauth2/disconnect", Perm("email_settings"), "Forget the connected OAuth2 mail account",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!EmailService::UsesOAuth2()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "OAuth2 is not in use");
			const auto requestId = PlayerActions::Begin(context.accountId);
			EmailService::OAuth2Disconnect([requestId](const std::optional<std::string>& error) {
				PlayerActions::Finish(requestId, error ? PlayerActions::Outcome{ false, *error } : PlayerActions::Outcome{ true, "Mail account disconnected" });
			});
			Audit(context, "disconnect_mail_account", "Disconnected the OAuth2 mail account the dashboard sends email with");
			JsonSuccess(reply, { {"requestId", requestId} });
		});

	Route(eHTTPMethod::POST, "/api/email/test", Perm("email_settings"), "Send a test email to check SMTP settings. Body: {to}",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!EmailService::IsConfigured()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Email is not configured: set smtp_host, smtp_from_address and dashboard_url");
			const auto body = ParseBody(context);
			const std::string to = body ? body->value("to", "") : "";
			if (!Smtp::IsValidAddress(to)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Enter a valid address to send the test to");

			Smtp::Message message;
			message.to = to;
			message.subject = ServerName() + " test email";
			message.body = "This is a test email from the " + ServerName() + " dashboard, sent by " + context.authenticatedUser +
				".\n\nIf you can read this, email is set up correctly.\n";
			Audit(context, "send_test_email", to);
			JsonSuccess(reply, { {"requestId", SendTracked(std::move(message), "Test email accepted by the mail server for " + to, context.accountId)} });
		});
}
