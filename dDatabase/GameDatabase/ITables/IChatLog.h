#ifndef __ICHATLOG__H__
#define __ICHATLOG__H__

#include <cstdint>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "json.hpp"

/**
 * What players say: zone chat (world servers), whispers and team chat (chat server), and messages sent into the game
 * from the dashboard or a chat bridge. Read by the dashboard's chat log and its API for bots.
 */
class IChatLog {
public:
	struct ChatMessage {
		uint64_t id{};
		int64_t time{};
		std::string channel;       // zone, whisper, team, guild, web
		LWOOBJID senderId{};       // the character; 0 for messages from the web
		std::string senderName;
		uint32_t accountId{};
		LWOOBJID recipientId{};    // whispers
		std::string recipientName;
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
		std::string message;
		bool blocked{};            // stopped by the chat filter: nobody saw it
		int64_t guildId{};         // guild chat: the guild
		LWOOBJID teamId{};         // team chat: the chat server's team
		bool filtered{};           // the chat filter found words it doesn't allow (delivered anyway unless blocked)
	};

	struct ChatQuery {
		uint64_t afterId{};        // only messages newer than this (for bots catching up)
		uint32_t limit{ 100 };
		std::string channel;       // empty: any
		bool includePrivate{};     // team and guild chat
		bool includeWhispers{};    // whispers
		LWOOBJID characterId{};    // sent or received by this character (0: anyone)
		LWOOBJID otherCharacterId{}; // with characterId: only whispers between the two
		uint32_t accountId{};      // sent by this account (0: anyone)
		uint32_t zoneId{};         // 0: any
		int64_t instanceId{ -1 };  // -1: any (instance 0 is character select)
		int64_t since{};           // only messages at or after this time (0: any)
		int64_t until{};           // only messages before this time (0: any)
		uint64_t beforeId{};       // only messages older than this (0: any; for paging back through a conversation)
		int64_t guildId{};         // 0: any
		LWOOBJID teamId{};         // 0: any
		std::string search;        // text in the message or a name
		bool blockedOnly{};
		bool newestFirst{};
		uint32_t offset{};
	};

	virtual uint64_t InsertChatMessage(const ChatMessage& message) = 0;
	virtual std::vector<ChatMessage> GetChatMessages(const ChatQuery& query) = 0;
	virtual uint64_t CountChatMessages(const ChatQuery& query) = 0;

	// The characters a character whispered with, most recent conversation first
	struct WhisperPartner {
		LWOOBJID characterId{};
		std::string name;          // the name in their newest message
		uint64_t messages{};
		int64_t firstTime{};
		int64_t lastTime{};
	};
	virtual std::vector<WhisperPartner> GetWhisperPartners(LWOOBJID characterId, uint32_t offset, uint32_t limit) = 0;
	virtual uint64_t CountWhisperPartners(LWOOBJID characterId) = 0;

	// Teams that talked in team chat, most recent first; characterId (0: any) only teams that character talked in
	struct ChatTeam {
		LWOOBJID teamId{};
		uint64_t messages{};
		int64_t firstTime{};
		int64_t lastTime{};
		std::string senders;       // the names that talked, comma separated
	};
	virtual std::vector<ChatTeam> GetChatTeams(LWOOBJID characterId, uint32_t offset, uint32_t limit) = 0;
	virtual uint64_t CountChatTeams(LWOOBJID characterId) = 0;
};

#endif  //!__ICHATLOG__H__
