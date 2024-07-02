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
};

#endif  //!__IACCOUNTS__H__
