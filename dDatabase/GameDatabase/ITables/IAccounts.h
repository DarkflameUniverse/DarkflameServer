#ifndef __IACCOUNTS__H__
#define __IACCOUNTS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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
	virtual void InsertNewAccount(const std::string_view username, const std::string_view bcryptpassword) = 0;

	// Update the GameMaster level of an account.
	virtual void UpdateAccountGmLevel(const uint32_t accountId, const eGameMasterLevel gmLevel) = 0;

	virtual uint32_t GetAccountCount() = 0;

	// Login attempt tracking methods
	// Record a failed login attempt
	virtual void RecordFailedAttempt(const uint32_t accountId) = 0;

	// Clear failed login attempts and update last login time
	virtual void ClearFailedAttempts(const uint32_t accountId) = 0;

	// Set account lockout
	virtual void SetLockout(const uint32_t accountId, const int64_t lockoutUntil) = 0;

	// Check if account is locked out
	virtual bool IsLockedOut(const uint32_t accountId) = 0;

	// Get failed attempt count
	virtual uint8_t GetFailedAttempts(const uint32_t accountId) = 0;
};

#endif  //!__IACCOUNTS__H__
