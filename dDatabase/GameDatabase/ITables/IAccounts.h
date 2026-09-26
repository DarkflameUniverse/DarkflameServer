#ifndef __IACCOUNTS__H__
#define __IACCOUNTS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "json.hpp"

enum class eGameMasterLevel : uint8_t;

class IAccounts {
public:
	struct Info {
		std::string bcryptPassword;
		uint32_t id{};
		uint32_t playKeyId{};
		uint64_t muteExpire{};
		bool banned{};
		bool locked{};
		int64_t banExpires{}; // unix time a temporary ban ends; 0 for a permanent one
		std::string banReason;
		eGameMasterLevel maxGmLevel{};
	};

	// Get the account info for the given username.
	virtual std::optional<IAccounts::Info> GetAccountInfo(const std::string_view username) = 0;

	// Update the account's unmute time.
	virtual void UpdateAccountUnmuteTime(const uint32_t accountId, const uint64_t timeToUnmute) = 0;

	// Update the account's ban status.
	virtual void UpdateAccountBan(const uint32_t accountId, const bool banned) = 0;

	// Update the account's password.
	virtual void UpdateAccountPassword(const uint32_t accountId, const std::string_view bcryptpassword) = 0;

	// Add a new account to the database.
	virtual void InsertNewAccount(const std::string_view username, const std::string_view bcryptpassword, const eGameMasterLevel gmLevel) = 0;

	// Update the GameMaster level of an account.
	virtual void UpdateAccountGmLevel(const uint32_t accountId, const eGameMasterLevel gmLevel) = 0;

	virtual uint32_t GetAccountCount() = 0;

	// Login attempt tracking methods
	// Record a failed login attempt
	virtual void RecordFailedAttempt(const uint32_t accountId) = 0;

	// Clear failed login attempts and update last login time
	virtual void ClearFailedAttempts(const uint32_t accountId) = 0;

	// Throttle logins to the account until the given unix time (after too many failed attempts) and start a new failure count.
	// This is not the staff lock; see SetAccountLocked. 0 clears the lockout.
	virtual void SetLockout(const uint32_t accountId, const int64_t lockoutUntil) = 0;

	// Whether the account's failed-login lockout (SetLockout) is still running
	virtual bool IsLockedOut(const uint32_t accountId) = 0;

	// Staff lock (accounts.locked): a locked account can't log in to the game or the dashboard until unlocked
	virtual void SetAccountLocked(const uint32_t accountId, const bool locked) = 0;

	// Get failed attempt count
	virtual uint8_t GetFailedAttempts(const uint32_t accountId) = 0;

	// Get paginated list of accounts with optional search/filtering for DataTables
	// Returns a JSON object with the account data and metadata
	virtual nlohmann::json GetAccountsTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) = 0;

	// Get a single account by ID
	// Returns a JSON object with the account details
	virtual nlohmann::json GetAccountById(uint32_t accountId) = 0;

	// Get characters for an account as JSON for dashboard display
	virtual nlohmann::json GetAccountCharacters(uint32_t accountId) = 0;

	// Delete an account and all related data
	virtual void DeleteAccount(const uint32_t accountId) = 0;

	// How many accounts at exactly this GM level can still sign in (not banned or locked), leaving out excludeAccountId
	virtual uint32_t CountActiveAccountsAtGmLevel(const uint8_t gmLevel, const uint32_t excludeAccountId) = 0;
};

#endif  //!__IACCOUNTS__H__
