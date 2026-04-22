#ifndef __IACCOUNTS__H__
#define __IACCOUNTS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

	// Set the play_key_id for an account (used during registration)
	virtual void UpdateAccountPlayKey(const uint32_t accountId, const uint32_t playKeyId) = 0;

	// Get counts for dashboard/stats
	virtual uint32_t GetBannedAccountCount() = 0;
	virtual uint32_t GetLockedAccountCount() = 0;

	virtual uint32_t GetAccountCount() = 0;

	struct ListInfo {
		uint32_t id{};
		std::string name;
		eGameMasterLevel gm_level{};
		bool banned{};
		bool locked{};
		uint64_t mute_expire{};
		uint32_t play_key_id{};
	};

	struct DetailedInfo {
		uint32_t id{};
		std::string name;
		std::string email;
		eGameMasterLevel gm_level{};
		bool banned{};
		bool locked{};
		uint64_t mute_expire{};
		uint32_t play_key_id{};
		uint64_t created_at{};
	};

	struct SessionInfo {
		uint64_t sessionId{};
		std::string ipAddress;
		uint64_t loginTime{};
		uint64_t logoutTime{};
		bool active{};
	};

	// Return all accounts for dashboard listing
	virtual std::vector<ListInfo> GetAllAccounts() = 0;

	// Update an account's locked status
	virtual void UpdateAccountLock(const uint32_t accountId, const bool locked) = 0;

	// Get detailed account info by ID (for dashboard viewing)
	virtual std::optional<DetailedInfo> GetAccountById(const uint32_t accountId) = 0;

	// Update account email (for dashboard)
	virtual void UpdateAccountEmail(const uint32_t accountId, const std::string_view email) = 0;

	// Delete account and all associated data
	virtual void DeleteAccount(const uint32_t accountId) = 0;

	// Get account session history
	virtual std::vector<SessionInfo> GetAccountSessions(const uint32_t accountId, uint32_t limit = 50) = 0;
};

#endif  //!__IACCOUNTS__H__
