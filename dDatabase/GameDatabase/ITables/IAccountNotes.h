#ifndef __IACCOUNTNOTES__H__
#define __IACCOUNTNOTES__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * An account's moderation history: notes and warnings staff write, and every ban, mute and lock with its reason.
 * Also temporary bans: a ban with an end time is lifted at login (and by an hourly task) once it has passed.
 */
class IAccountNotes {
public:
	struct AccountNote {
		uint64_t id{};
		uint32_t accountId{};
		std::string kind; // note, warning, ban, unban, mute, unmute, lock, unlock
		std::string text;
		std::string actor;
		int64_t createdAt{};
	};

	virtual void InsertAccountNote(const AccountNote& note) = 0;
	virtual std::vector<AccountNote> GetAccountNotes(uint32_t accountId) = 0; // newest first
	virtual std::optional<AccountNote> GetAccountNote(uint64_t id) = 0;
	virtual void DeleteAccountNote(uint64_t id) = 0;
	virtual uint32_t GetAccountWarningCount(uint32_t accountId) = 0;

	// Ban or unban with an end time (0: permanent) and a reason shown to the player
	virtual void SetAccountBan(uint32_t accountId, bool banned, int64_t expires, const std::string& reason) = 0;

	// Unban every temporary ban that ended before `now`; returns their account ids
	virtual std::vector<uint32_t> LiftExpiredBans(int64_t now) = 0;
};

#endif  //!__IACCOUNTNOTES__H__
