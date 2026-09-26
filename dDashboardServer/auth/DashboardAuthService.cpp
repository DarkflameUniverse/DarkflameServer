#include "DashboardAuthService.h"
#include "JWTUtils.h"
#include "Database.h"
#include "Logger.h"
#include "Game.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "Totp.h"
#include "LoginThrottle.h"
#include <bcrypt/bcrypt.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <algorithm>
#include <ctime>
#include <map>
#include <vector>

namespace {
	// Failed sign-ins are throttled per (address, account): a few wrong guesses from one address block only that
	// address from that account for a while, so nobody can lock the owner out of their own account.
	constexpr uint32_t ADDRESS_MAX_FAILURES = 5;
	constexpr int64_t ADDRESS_WINDOW = 15 * 60;
	constexpr int64_t ADDRESS_BLOCK = 15 * 60;
	// Backstop against guessing from many addresses: the account-wide lockout (accounts.lockout_time) is short and
	// needs many failures without a successful sign-in in between.
	constexpr uint8_t ACCOUNT_MAX_FAILURES = 25;
	constexpr int64_t ACCOUNT_LOCKOUT_DURATION = 10 * 60;
	constexpr int64_t CHALLENGE_LIFETIME = 5 * 60;
	constexpr int CHALLENGE_ATTEMPTS = 5;

	// A real bcrypt hash (same work factor as accounts) to check against when the username doesn't exist
	const std::string& TimingDummyHash() {
		static const std::string hash = [] {
			char salt[BCRYPT_HASHSIZE];
			char out[BCRYPT_HASHSIZE];
			if (::bcrypt_gensalt(12, salt) != 0 || ::bcrypt_hashpw("dashboard timing dummy", salt, out) != 0) return std::string();
			return std::string(out);
		}();
		return hash;
	}

	struct Challenge {
		uint32_t accountId{};
		std::string username;
		bool rememberMe{};
		int64_t expires{};
		int attempts{};
		std::string address;
	};
	std::map<std::string, Challenge> g_Challenges; // by hash of the challenge token

	std::string RandomHex(size_t bytes) {
		std::vector<unsigned char> data(bytes);
		if (RAND_bytes(data.data(), static_cast<int>(data.size())) != 1) return "";
		static constexpr char digits[] = "0123456789abcdef";
		std::string out;
		for (const auto b : data) { out += digits[b >> 4]; out += digits[b & 15]; }
		return out;
	}

	LoginThrottle g_Throttle(ADDRESS_MAX_FAILURES, ADDRESS_WINDOW, ADDRESS_BLOCK);

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	std::string BlockedMessage(int64_t seconds) {
		const auto minutes = std::max<int64_t>(1, (seconds + 59) / 60);
		return "Too many failed attempts, try again in " + std::to_string(minutes) + " minute" + (minutes == 1 ? "" : "s");
	}

	// Wrong passwords, two-factor codes and recovery attempts all count here. Returns whether the caller is now blocked.
	bool RecordFailure(uint32_t accountId, const std::string& username, const std::string& address) {
		const bool addressBlocked = g_Throttle.RecordFailure(address, accountId, Now());
		if (addressBlocked) LOG("Sign-in to %s blocked for %s after failed attempts", username.c_str(), address.c_str());
		Database::Get()->RecordFailedAttempt(accountId);
		if (Database::Get()->GetFailedAttempts(accountId) >= ACCOUNT_MAX_FAILURES) {
			// SetLockout also starts a new count, so this can't turn into a permanent lock
			Database::Get()->SetLockout(accountId, Now() + ACCOUNT_LOCKOUT_DURATION);
			LOG("Account %s temporarily locked after %d failed attempts from all addresses", username.c_str(), ACCOUNT_MAX_FAILURES);
			return true;
		}
		return addressBlocked;
	}

	// Seconds the caller must still wait before trying this account, 0 when it may try
	int64_t Throttled(uint32_t accountId, const std::string& address) {
		if (const auto wait = g_Throttle.BlockedFor(address, accountId, Now())) return wait;
		return Database::Get()->IsLockedOut(accountId) ? ACCOUNT_LOCKOUT_DURATION : 0;
	}
}

DashboardAuthService::LoginResult DashboardAuthService::Login(
	const std::string& username, 
	const std::string& password, 
	bool rememberMe,
	const std::string& address) {

	LoginResult result;

	if (username.empty() || password.empty()) {
		result.message = "Username and password are required";
		return result;
	}

	if (password.length() > 40) {
		result.message = "Password exceeds maximum length (40 characters)";
		return result;
	}

	try {
		// Get account info
		auto accountInfo = Database::Get()->GetAccountInfo(username);
		if (!accountInfo) {
			// Take as long as a wrong password would, so response times don't reveal which usernames exist
			::bcrypt_checkpw(password.c_str(), TimingDummyHash().c_str());
			result.message = "Invalid username or password";
			LOG_DEBUG("Login attempt for non-existent user: %s", username.c_str());
			return result;
		}

		uint32_t accountId = accountInfo->id;

		// While throttled the password isn't checked at all (and the wait isn't extended)
		if (const auto wait = Throttled(accountId, address)) {
			result.message = BlockedMessage(wait);
			result.accountLocked = true;
			LOG_DEBUG("Throttled login attempt for %s from %s", username.c_str(), address.c_str());
			return result;
		}

		// Check password
		if (::bcrypt_checkpw(password.c_str(), accountInfo->bcryptPassword.c_str()) != 0) {
			if (RecordFailure(accountId, username, address)) {
				result.message = BlockedMessage(ADDRESS_BLOCK);
				result.accountLocked = true;
			} else {
				result.message = "Invalid username or password";
				LOG_DEBUG("Failed login attempt for user: %s from %s", username.c_str(), address.c_str());
			}
			return result;
		}

		if (accountInfo->banned) {
			result.message = "Account is banned";
			return result;
		}

		if (accountInfo->locked) {
			result.message = "Account is locked";
			return result;
		}

		// Check GM level
		if (!HasDashboardAccess(static_cast<uint8_t>(accountInfo->maxGmLevel))) {
			result.message = "Access denied: insufficient permissions";
			LOG("Access denied for non-admin user: %s", username.c_str());
			return result;
		}

		// With two-factor login on, the password alone is not enough: hand out a short-lived challenge
		if (Database::Get()->GetTotp(accountId).enabledAt != 0) {
			const auto now = static_cast<int64_t>(std::time(nullptr));
			std::erase_if(g_Challenges, [now](const auto& entry) { return entry.second.expires < now; });
			result.challenge = RandomHex(32);
			if (result.challenge.empty()) {
				result.message = "An error occurred during login";
				return result;
			}
			g_Challenges[Sha256Hex(result.challenge)] = { accountId, username, rememberMe, now + CHALLENGE_LIFETIME, 0, address };
			result.twoFactorRequired = true;
			result.accountId = accountId;
			result.message = "Enter the code from your authenticator app";
			return result;
		}

		// Successful login
		Database::Get()->ClearFailedAttempts(accountId);
		g_Throttle.Clear(address, accountId);
		result.accountId = accountId;
		result.success = true;
		result.gmLevel = static_cast<uint8_t>(accountInfo->maxGmLevel);
		result.token = JWTUtils::GenerateSessionToken(accountId, username, result.gmLevel, rememberMe);
		if (result.token.empty()) {
			result.success = false;
			result.message = "Server authentication is not configured";
			return result;
		}
		result.message = "Login successful";

		LOG("Successful login: %s (GM Level: %d)", username.c_str(), result.gmLevel);
		return result;

	} catch (const std::exception& ex) {
		result.message = "An error occurred during login";
		LOG("Error during login process: %s", ex.what());
		return result;
	}
}

bool DashboardAuthService::VerifyToken(const std::string& token, std::string& username, uint8_t& gmLevel, uint32_t& accountId) {
	JWTUtils::JWTPayload payload;
	if (!JWTUtils::ValidateToken(token, payload)) {
		LOG_DEBUG("Token validation failed: invalid or expired JWT");
		return false;
	}

	try {
		auto accountInfo = Database::Get()->GetAccountInfo(payload.username);
		if (!accountInfo || accountInfo->banned || accountInfo->locked || !HasDashboardAccess(static_cast<uint8_t>(accountInfo->maxGmLevel))) {
			LOG_DEBUG("Token verification failed: user no longer has access");
			return false;
		}
		// A token only works for the account it was issued to, not a later one with the same name
		if (payload.accountId != accountInfo->id) {
			LOG_DEBUG("Token verification failed: token belongs to a different account");
			return false;
		}
		// Sessions issued before a password reset or change are no longer valid
		if (payload.issuedAt < Database::Get()->GetSessionsValidAfter(accountInfo->id)) {
			LOG_DEBUG("Token verification failed: session was revoked");
			return false;
		}
		username = payload.username;
		gmLevel = static_cast<uint8_t>(accountInfo->maxGmLevel);
		accountId = accountInfo->id;
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error verifying user during token validation: %s", ex.what());
		return false;
	}

	LOG_DEBUG("Token verified successfully for user: %s (GM Level: %d)", username.c_str(), gmLevel);
	return true;
}

bool DashboardAuthService::HasDashboardAccess(uint8_t gmLevel) {
	// Get minimum GM level from config (default 0 = any user)
	uint8_t minGmLevel = 0;
	
	if (Game::config) {
		const std::string& minGmLevelStr = Game::config->GetValue("min_dashboard_gm_level");
		if (!minGmLevelStr.empty()) {
			const auto parsed = GeneralUtils::TryParse<uint8_t>(minGmLevelStr);
			if (parsed) {
				minGmLevel = parsed.value();
			}
		}
	}
	
	return gmLevel >= minGmLevel;
}

DashboardAuthService::LoginResult DashboardAuthService::CompleteTwoFactor(const std::string& challenge, const std::string& code) {
	LoginResult result;
	const auto now = static_cast<int64_t>(std::time(nullptr));
	const auto it = g_Challenges.find(Sha256Hex(challenge));
	if (challenge.empty() || it == g_Challenges.end() || it->second.expires < now) {
		if (it != g_Challenges.end()) g_Challenges.erase(it);
		result.message = "This login has expired, please sign in again";
		return result;
	}
	auto& pending = it->second;

	try {
		const auto account = Database::Get()->GetAccountInfo(pending.username);
		if (!account || account->id != pending.accountId || account->banned || account->locked || !HasDashboardAccess(static_cast<uint8_t>(account->maxGmLevel))) {
			g_Challenges.erase(it);
			result.message = "Access denied";
			return result;
		}
		if (const auto wait = Throttled(account->id, pending.address)) {
			g_Challenges.erase(it);
			result.message = BlockedMessage(wait);
			result.accountLocked = true;
			return result;
		}

		bool usedRecovery = false;
		if (!CheckTwoFactorCode(account->id, code, true, &usedRecovery)) {
			result.accountLocked = RecordFailure(account->id, pending.username, pending.address);
			if (result.accountLocked || ++pending.attempts >= CHALLENGE_ATTEMPTS) g_Challenges.erase(it);
			result.message = result.accountLocked ? BlockedMessage(ADDRESS_BLOCK) : "That code is not right";
			return result;
		}

		const auto username = pending.username;
		const auto rememberMe = pending.rememberMe;
		const auto address = pending.address;
		g_Challenges.erase(it);
		Database::Get()->ClearFailedAttempts(account->id);
		g_Throttle.Clear(address, account->id);
		result.gmLevel = static_cast<uint8_t>(account->maxGmLevel);
		result.token = JWTUtils::GenerateSessionToken(account->id, username, result.gmLevel, rememberMe);
		if (result.token.empty()) {
			result.message = "Server authentication is not configured";
			return result;
		}
		result.success = true;
		result.usedRecoveryCode = usedRecovery;
		result.accountId = account->id;
		result.message = "Login successful";
		LOG("Successful two-factor login: %s (GM Level: %d)%s", username.c_str(), result.gmLevel, usedRecovery ? " using a recovery code" : "");
	} catch (const std::exception& ex) {
		result.message = "An error occurred during login";
		LOG("Error during two-factor login: %s", ex.what());
	}
	return result;
}

bool DashboardAuthService::CheckTwoFactorCode(uint32_t accountId, const std::string& code, bool allowRecovery, bool* usedRecoveryCode) {
	if (usedRecoveryCode) *usedRecoveryCode = false;
	const auto totp = Database::Get()->GetTotp(accountId);
	if (totp.enabledAt == 0 || totp.encryptedSecret.empty()) return false;

	const auto secret = Totp::DecryptSecret(totp.encryptedSecret);
	if (!secret) {
		LOG("Could not decrypt the two-factor secret of account %u (was dashboard_totp_key replaced?)", accountId);
	} else if (const auto step = Totp::Verify(*secret, code, static_cast<int64_t>(std::time(nullptr)))) {
		// A code seen once can't be replayed, even inside its 90 second window
		return Database::Get()->UseTotpStep(accountId, *step);
	}

	if (!allowRecovery) return false;
	const auto normalized = Totp::NormalizeRecoveryCode(code);
	if (normalized.size() != 12) return false;
	if (!Database::Get()->UseRecoveryCode(accountId, Sha256Hex(normalized))) return false;
	if (usedRecoveryCode) *usedRecoveryCode = true;
	return true;
}

bool DashboardAuthService::RecordFailedAttempt(uint32_t accountId, const std::string& username, const std::string& address) {
	return RecordFailure(accountId, username, address);
}

bool DashboardAuthService::IsThrottled(uint32_t accountId, const std::string& address) {
	return Throttled(accountId, address) != 0;
}

void DashboardAuthService::ClearThrottle(uint32_t accountId) {
	g_Throttle.ClearAccount(accountId);
}

uint8_t DashboardAuthService::RequiredTwoFactorLevel() {
	if (!Game::config) return 0;
	return GeneralUtils::TryParse<uint8_t>(Game::config->GetValue("require_2fa_gm_level")).value_or(0);
}

bool DashboardAuthService::NeedsTwoFactorSetup(uint32_t accountId, uint8_t gmLevel) {
	const auto required = RequiredTwoFactorLevel();
	if (required == 0 || gmLevel < required) return false;
	return Database::Get()->GetTotp(accountId).enabledAt == 0;
}

std::string DashboardAuthService::Sha256Hex(const std::string& text) {
	unsigned char digest[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(text.data()), text.size(), digest);
	static constexpr char digits[] = "0123456789abcdef";
	std::string out;
	for (const auto b : digest) { out += digits[b >> 4]; out += digits[b & 15]; }
	return out;
}
