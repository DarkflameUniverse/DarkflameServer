#ifndef __ICHATFLAGS__H__
#define __ICHATFLAGS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * Chat messages staff flagged for review (the Chat Flags page). A flag covers one message or several from one
 * conversation, keeps a copy of them and the chat around them (chat_log is pruned after log_chat_days; the flag keeps
 * its evidence), and has a history of who did what with it.
 */
class IChatFlags {
public:
	struct ChatFlag {
		uint64_t id{};
		int64_t createdAt{};
		uint32_t createdById{};     // the staff account
		std::string createdBy;
		std::string status{ "open" }; // open, actioned, dismissed
		std::string channel;        // of the flagged messages
		LWOOBJID characterId{};     // who the flag is about
		std::string characterName;
		uint32_t accountId{};
		uint64_t firstMessageId{};
		uint64_t lastMessageId{};
		int64_t firstTime{};
		int64_t lastTime{};
		std::string excerpt;        // the flagged text, shortened, for the queue
		std::string note;           // why it was flagged, and what was done
		std::string messages;       // JSON array: the flagged messages and the chat around them, as they were
		uint64_t playerReportId{};  // a player report this goes with (0: none)
		int64_t updatedAt{};
		std::string updatedBy;
	};

	struct ChatFlagEvent {
		uint64_t id{};
		uint64_t flagId{};
		int64_t time{};
		uint32_t accountId{};
		std::string actor;
		std::string action;         // created, actioned, dismissed, reopened, note, report
		std::string detail;
	};

	struct ChatFlagQuery {
		std::string status;         // empty: any
		LWOOBJID characterId{};     // 0: anyone
		uint32_t accountId{};       // 0: any
		bool includePrivate{};      // flags on team and guild chat
		bool includeWhispers{};     // flags on whispers
		uint32_t offset{};
		uint32_t limit{ 50 };
	};

	// Adds the flag and remembers which messages it covers. Returns its id.
	virtual uint64_t InsertChatFlag(const ChatFlag& flag, const std::vector<uint64_t>& messageIds) = 0;
	virtual std::optional<ChatFlag> GetChatFlag(uint64_t id) = 0;
	// Newest first
	virtual std::vector<ChatFlag> GetChatFlags(const ChatFlagQuery& query) = 0;
	virtual uint64_t CountChatFlags(const ChatFlagQuery& query) = 0;
	// Status, note and linked report; returns false when there is no such flag
	virtual bool UpdateChatFlag(uint64_t id, const std::string& status, const std::string& note, uint64_t playerReportId, int64_t time, const std::string& updatedBy) = 0;
	virtual void InsertChatFlagEvent(const ChatFlagEvent& event) = 0;
	// Oldest first
	virtual std::vector<ChatFlagEvent> GetChatFlagEvents(uint64_t flagId) = 0;
	// Of these messages, the ones in a flag, with the newest flag covering each
	virtual std::vector<std::pair<uint64_t, uint64_t>> GetFlaggedMessages(const std::vector<uint64_t>& messageIds) = 0;
};

#endif  //!__ICHATFLAGS__H__
