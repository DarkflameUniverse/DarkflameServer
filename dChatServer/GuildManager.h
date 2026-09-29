#ifndef GUILDMANAGER_H
#define GUILDMANAGER_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "dCommonVars.h"
#include "eGuildRank.h"
#include "IGuilds.h"

struct LUBitStream;

/**
 * The chat server's guilds (docs/Guilds.md): it is the guild authority. State is in the database (IGuilds); what it
 * tells clients goes through their worlds. It only runs on the chat server's main thread.
 *
 * Everything outside the guild tables is reached through Hooks, so the rules can be tested without a server.
 */
class GuildManager {
public:
	// What the chat server knows about an online player
	struct OnlinePlayer {
		LWOOBJID id{};
		std::string name;
		LWOZONEID zone{};
	};

	// The chat filter's verdict on a guild name
	enum class NameCheck {
		APPROVED, // on the allow list
		PENDING,  // not on the deny list, not (all) on the allow list: a moderator decides
		DENIED,   // on the deny list
	};

	struct Hooks {
		std::function<std::optional<OnlinePlayer>(LWOOBJID)> findOnline;
		std::function<std::optional<OnlinePlayer>(const std::string&)> findOnlineByName;
		// Whether a character with this name exists (for "not online" versus "could not invite")
		std::function<bool(const std::string&)> characterExists;
		// A client packet to an online player (routed through their world)
		std::function<void(LWOOBJID, const LUBitStream&)> sendToPlayer;
		// A chat -> world packet to the world an online player is in
		std::function<void(LWOOBJID, const LUBitStream&)> sendToWorldOf;
		// A system chat line to an online player
		std::function<void(LWOOBJID, const std::string&)> notify;
		std::function<NameCheck(const std::string&)> checkName;
		std::function<int64_t()> now;
	};

	struct Settings {
		uint32_t maxMembers = 100;
		int64_t inviteTimeout = 600; // seconds an invite can be answered
	};

	GuildManager(IGuilds& db, Hooks hooks, Settings settings);

	// GuildNameRules::IsValid
	static bool IsValidName(const std::u16string& name);
	// The client's rank rules as DLU sets them (the client lets anyone try: GuildCanWeInvite/Kick are always true)
	static bool CanInvite(eGuildRank rank);
	static bool CanKick(eGuildRank actor, eGuildRank target);
	static bool CanSetRank(eGuildRank actor, eGuildRank target, eGuildRank newRank);

	// From the client (through its world)
	void Create(LWOOBJID playerID, const std::u16string& name);
	void Invite(LWOOBJID playerID, const std::string& targetName);
	void AnswerInvite(LWOOBJID playerID, bool declined);
	void Leave(LWOOBJID playerID);
	void GetAll(LWOOBJID playerID);
	// DLU slash commands (the client has no controls for them)
	void Kick(LWOOBJID playerID, const std::string& targetName);
	void SetRank(LWOOBJID playerID, const std::string& targetName, eGuildRank rank);
	void Disband(LWOOBJID playerID);

	// A player came online (login) or changed worlds
	void PlayerOnline(LWOOBJID playerID, bool login);
	// A player went offline (before the chat server forgets them)
	void PlayerOffline(LWOOBJID playerID);

	// The dashboard changed a guild in the database (renamed, name moderated, disbanded, member removed): tell the
	// online players what changed for them
	void GuildChanged(int64_t guildID);

	// Online members of the player's guild, the player included (empty: not in a guild)
	std::vector<LWOOBJID> OnlineGuildmates(LWOOBJID playerID);

	// The name other players see: nothing while it waits for moderation
	static std::string ShownName(const IGuilds::Guild& guild);
	// MM/DD/YYYY, UTC
	static std::string FormatDate(int64_t time);

private:
	void SendStatus(LWOOBJID characterID, int64_t guildID, const std::string& shownName);
	void SendData(LWOOBJID playerID);
	void SendDataToOnline(int64_t guildID);
	void LogEvent(int64_t guildID, const std::string& kind, LWOOBJID characterID, const std::string& characterName, const std::string& actor, const std::string& detail = "");
	// The members, with a leader again if the guild lost its (the leader's character was deleted). Empty: the guild is
	// gone (deleted when it had no members left).
	std::vector<IGuilds::Member> Members(int64_t guildID);
	std::optional<IGuilds::Member> FindMember(const std::vector<IGuilds::Member>& members, const std::string& name) const;
	// Removes a member and tells everyone; hands the guild on when the leader goes, deletes it with the last member
	void RemoveMember(const IGuilds::Guild& guild, const IGuilds::Member& member, eGuildLeaveReason reason, const std::string& actor);
	std::string NameOf(LWOOBJID playerID, const std::string& fallback = "");

	IGuilds& m_Db;
	Hooks m_Hooks;
	Settings m_Settings;
	// Online players' guilds, to know who to tell when the dashboard changes a guild behind the chat server's back
	std::unordered_map<LWOOBJID, int64_t> m_OnlineGuild;
};

#endif // GUILDMANAGER_H
