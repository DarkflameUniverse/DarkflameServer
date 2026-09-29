#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "IChatLog.h"
#include "json.hpp"

/**
 * The parts of the chat histories and chat flags that don't touch the database or the web server: who may read which
 * channel, which chat counts as one conversation, and what a flag keeps. The routes are in ChatRoutes.cpp and
 * ChatFlagRoutes.cpp.
 */
namespace ChatHistory {
	// What a viewer may read besides zone and web chat (chat_view): team and guild chat (chat_private), whispers (chat_dms)
	struct Access {
		bool group{};
		bool whispers{};
	};

	bool CanRead(const std::string& channel, const Access& access);

	// A message as the dashboard sends it. Text of a channel the viewer may not read is left out (redacted: true).
	nlohmann::json MessageJson(const IChatLog::ChatMessage& message, const Access& access);

	// The query for the conversation a message belongs to: the same world for zone chat, the same two characters for
	// whispers, the same team or guild, or chat from the web
	IChatLog::ChatQuery ConversationQuery(const IChatLog::ChatMessage& message, const Access& access);

	// Whether two messages are in the same conversation (a flag covers messages of one conversation)
	bool SameConversation(const IChatLog::ChatMessage& a, const IChatLog::ChatMessage& b);

	// open, actioned or dismissed
	std::optional<std::string> ParseStatus(const std::string& status);

	// The history entry for a status change: actioned, dismissed or reopened (empty when it didn't change)
	std::string StatusAction(const std::string& from, const std::string& to);

	// "Name: text / Name: text", at most maxBytes (cut between UTF-8 characters, with an ellipsis)
	std::string Excerpt(const std::vector<IChatLog::ChatMessage>& messages, size_t maxBytes = 500);

	// What a flag keeps: the chat before, the flagged messages (flagged: true) and the chat after, oldest first
	nlohmann::json Snapshot(const std::vector<IChatLog::ChatMessage>& before, const std::vector<IChatLog::ChatMessage>& flagged,
		const std::vector<IChatLog::ChatMessage>& after);

	// A flag's copy of the chat for a viewer: text of channels they may not read is left out
	nlohmann::json RedactSnapshot(const nlohmann::json& snapshot, const Access& access);

	// Who a flag is about: the character given if they sent one of the messages, else the sender of the first message
	// from a character (web messages have none)
	std::optional<IChatLog::ChatMessage> Subject(const std::vector<IChatLog::ChatMessage>& flagged, LWOOBJID chosen);
}
