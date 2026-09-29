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
	};

	struct ChatQuery {
		uint64_t afterId{};        // only messages newer than this (for bots catching up)
		uint32_t limit{ 100 };
		std::string channel;       // empty: any
		bool includePrivate{};     // whispers and team chat
		LWOOBJID characterId{};    // sent or received by this character (0: anyone)
		uint32_t accountId{};      // sent by this account (0: anyone)
		uint32_t zoneId{};         // 0: any
		int64_t instanceId{ -1 };  // -1: any (instance 0 is character select)
		int64_t since{};           // only messages at or after this time (0: any)
		std::string search;        // text in the message or a name
		bool blockedOnly{};
		bool newestFirst{};
		uint32_t offset{};
	};

	virtual uint64_t InsertChatMessage(const ChatMessage& message) = 0;
	virtual std::vector<ChatMessage> GetChatMessages(const ChatQuery& query) = 0;
	virtual uint64_t CountChatMessages(const ChatQuery& query) = 0;
};

#endif  //!__ICHATLOG__H__
