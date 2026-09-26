#pragma once

#include <string>
#include <cstdint>

/**
 * Dashboard authentication service
 * Handles user login, password verification, and account lockout
 */
class DashboardAuthService {
public:
	/**
	 * Login result structure
	 */
	struct LoginResult {
		bool success{false};
		std::string message{};
		std::string token{}; // JWT token if successful
		uint8_t gmLevel{0}; // GM level if successful
		bool accountLocked{false}; // Account is locked out
		bool twoFactorRequired{false}; // Password was right; finish with CompleteTwoFactor(challenge, code)
		std::string challenge{};
		bool usedRecoveryCode{false};
		uint32_t accountId{0};
	};

	/**
	 * Attempt to log in with username and password
	 * @param username The username
	 * @param password The plaintext password (max 40 characters)
	 * @param rememberMe If true, extends token expiration to 30 days
	 * @param address The client's network address; failed attempts are throttled per (address, account)
	 * @return LoginResult with success status and JWT token if successful
	 */
	static LoginResult Login(const std::string& username, const std::string& password, bool rememberMe, const std::string& address);

	/**
	 * Second step of logging in to an account with two-factor login: a code from the authenticator app, or one of
	 * the account's recovery codes. A challenge is valid for 5 minutes and a few attempts.
	 */
	static LoginResult CompleteTwoFactor(const std::string& challenge, const std::string& code);

	/**
	 * Check a two-factor code for an account (authenticator code, or a recovery code when allowRecovery).
	 * Each authenticator code and recovery code is accepted only once.
	 */
	static bool CheckTwoFactorCode(uint32_t accountId, const std::string& code, bool allowRecovery, bool* usedRecoveryCode = nullptr);

	// Count a failed attempt (wrong password or code) from an address. Returns whether that address is now blocked
	// from the account (or the whole account is briefly locked after many failures from everywhere).
	static bool RecordFailedAttempt(uint32_t accountId, const std::string& username, const std::string& address);

	// Whether sign-in attempts to the account from this address are currently refused
	static bool IsThrottled(uint32_t accountId, const std::string& address);

	// Forget failed attempts against an account (staff unlocked it)
	static void ClearThrottle(uint32_t accountId);

	// GM level from which two-factor login is mandatory (config require_2fa_gm_level), 0 when it is not required
	static uint8_t RequiredTwoFactorLevel();

	// Whether this account must set up two-factor login before using the dashboard
	static bool NeedsTwoFactorSetup(uint32_t accountId, uint8_t gmLevel);

	static std::string Sha256Hex(const std::string& text);

	/**
	 * Verify that a token is valid and resolve the account it belongs to.
	 * The GM level comes from the database, not the token, so demotions and bans apply immediately.
	 * @param token The JWT token
	 * @param username Output parameter for the username
	 * @param gmLevel Output parameter for the account's current GM level
	 * @param accountId Output parameter for the account ID
	 * @return true if token is valid and the account may still use the dashboard
	 */
	static bool VerifyToken(const std::string& token, std::string& username, uint8_t& gmLevel, uint32_t& accountId);

	/**
	 * Check if user has required GM level for dashboard access
	 * @param gmLevel The user's GM level
	 * @return true if user can access dashboard (GM level > 0)
	 */
	static bool HasDashboardAccess(uint8_t gmLevel);
};
