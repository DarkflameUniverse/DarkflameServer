#ifndef __IGUILDS__H__
#define __IGUILDS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * Guilds (docs/Guilds.md). The chat server owns them: it is the only writer apart from the dashboard. A character is in
 * at most one guild and has at most one pending invite. The leader is the member with rank 1; there is no leader column.
 */
class IGuilds {
public:
	// guilds.name_status, the way pet_names.approved counts
	enum NameStatus : int32_t {
		NAME_PENDING = 1,  // waits for a moderator; other players see no guild name
		NAME_APPROVED = 2,
	};

	struct Guild {
		int64_t id{};
		std::string name;
		int32_t nameStatus{};
		LWOOBJID founderId{};
		int64_t createdAt{};
	};

	struct Member {
		LWOOBJID characterId{};
		int64_t guildId{};
		uint8_t rank{};         // 1 leader, 2 officer, 3 veteran, 4 recruit (the client's names)
		int64_t joinedAt{};
		std::string name;       // charinfo.name; empty when the character is gone
	};

	struct Invite {
		LWOOBJID characterId{}; // who is invited
		int64_t guildId{};
		LWOOBJID inviterId{};
		int64_t createdAt{};
	};

	// One line of a guild's history
	struct Event {
		uint64_t id{};
		int64_t guildId{};
		int64_t time{};
		std::string kind;          // created, joined, left, kicked, rank, leader, renamed, name_approved, name_rejected, disbanded
		LWOOBJID characterId{};    // who it was about (0: the guild itself)
		std::string characterName;
		std::string actor;         // the character or dashboard user who did it
		std::string detail;
	};

	struct GuildSummary {
		Guild guild;
		uint32_t memberCount{};
		LWOOBJID leaderId{};
		std::string leaderName;
	};

	struct GuildPage {
		uint32_t total{};    // every guild (or every pending one)
		uint32_t filtered{}; // matching the search
		std::vector<GuildSummary> guilds;
	};

	// Adds a guild (guild.id is not used) and returns its id
	virtual int64_t InsertGuild(const Guild& guild) = 0;
	virtual std::optional<Guild> GetGuild(int64_t guildId) = 0;
	// Names are unique without regard to case
	virtual std::optional<Guild> GetGuildByName(const std::string& name) = 0;
	virtual void SetGuildName(int64_t guildId, const std::string& name, int32_t nameStatus) = 0;
	// Removes the guild, its members and its invites. Its events stay.
	virtual void DeleteGuild(int64_t guildId) = 0;

	virtual void AddGuildMember(const Member& member) = 0;
	virtual void RemoveGuildMember(LWOOBJID characterId) = 0;
	virtual void SetGuildMemberRank(LWOOBJID characterId, uint8_t rank) = 0;
	virtual std::optional<Member> GetGuildMember(LWOOBJID characterId) = 0;
	// Ordered by rank, then who joined first
	virtual std::vector<Member> GetGuildMembers(int64_t guildId) = 0;

	// Replaces the character's invite
	virtual void SetGuildInvite(const Invite& invite) = 0;
	virtual std::optional<Invite> GetGuildInvite(LWOOBJID characterId) = 0;
	virtual void DeleteGuildInvite(LWOOBJID characterId) = 0;

	virtual uint64_t InsertGuildEvent(const Event& event) = 0;
	// Newest first; guildId 0: every guild's
	virtual std::vector<Event> GetGuildEvents(int64_t guildId, uint32_t limit) = 0;

	// Guilds for the dashboard, newest first. search matches the name, or the id when it is a number.
	virtual GuildPage GetGuildPage(uint32_t start, uint32_t length, const std::string& search, bool pendingOnly) = 0;
};

#endif  //!__IGUILDS__H__
