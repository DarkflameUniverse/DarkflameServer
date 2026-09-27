#ifndef __IMESSAGECAPTURES__H__
#define __IMESSAGECAPTURES__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * Saved captures of the dashboard's game message inspector (see dDashboardServer/routes/Inspector.h): each capture's
 * details and the messages it caught, with their raw bytes, so staff can look at them again after it ended. The
 * dashboard writes them while the capture runs and deletes old ones (the Message capture pruning task).
 */
class IMessageCaptures {
public:
	struct MessageCaptureSession {
		uint64_t id{};
		LWOOBJID characterId{};
		std::string characterName;
		uint32_t accountId{};
		std::string accountName;
		uint32_t startedById{};    // the staff account
		std::string startedBy;
		int64_t startedAt{};       // Unix seconds
		int64_t endsAt{};          // its time limit
		int64_t endedAt{};         // 0 while it runs
		std::string endReason;
		bool toServer{ true };
		bool toClient{ true };
		std::string onlyMessages;  // message IDs, comma separated
		std::string skipMessages;
		uint32_t zoneId{};         // where it was last captured
		uint32_t instanceId{};
		uint32_t cloneId{};
		std::string zones;         // every world it was captured in, in order: "zone:instance:clone" separated by spaces
		uint64_t messageCount{};
		uint64_t byteCount{};      // stored bytes (raw bytes plus decoded fields)
		uint64_t dropped{};        // messages the worlds left out (too many at once)
		uint8_t kind{};            // 0: game messages (message_capture_entries); 1: packets (a capture file, docs/CaptureReplay.md)
		std::string target;        // packets: "character", "account" or "everything"
	};

	// One captured message
	struct MessageCaptureRecord {
		uint64_t sessionId{};
		uint32_t seq{};            // from 1 in its session
		int64_t timeMs{};          // Unix milliseconds
		uint8_t direction{};       // eMessageDirection
		uint16_t messageId{};      // MessageType::Game
		LWOOBJID objectId{};
		uint32_t bits{};           // the message's full size
		uint32_t droppedBefore{};  // messages left out just before this one
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
		std::string payload;       // raw bytes after the object and message ID (may be cut short; see bits)
		std::string decoded;       // fields as JSON, or empty
	};

	// How GetMessageCaptureSessions sorts
	enum class eSessionOrder : uint8_t {
		STARTED,
		CHARACTER,
		STARTED_BY,
		MESSAGES,
		BYTES,
	};

	struct SessionQuery {
		LWOOBJID characterId{};    // 0: any
		uint32_t accountId{};      // the captured player's account (0: any)
		std::string startedBy;     // staff account name, any case (empty: anyone)
		int64_t since{};           // started at or after (0: any)
		int64_t until{};           // started before (0: any)
		bool unfinishedOnly{};     // still running (ended_at 0)
		eSessionOrder order{ eSessionOrder::STARTED };
		bool ascending{};
		uint32_t limit{ 50 };
		uint32_t offset{};
	};

	// Returns the new id
	virtual uint64_t InsertMessageCaptureSession(const MessageCaptureSession& session) = 0;

	// Everything that changes while it runs: ends_at, ended_at, end_reason, where, zones and the counts
	virtual void UpdateMessageCaptureSession(const MessageCaptureSession& session) = 0;

	// In one transaction
	virtual void InsertMessageCaptureEntries(const std::vector<MessageCaptureRecord>& entries) = 0;

	virtual std::optional<MessageCaptureSession> GetMessageCaptureSession(uint64_t id) = 0;
	virtual std::vector<MessageCaptureSession> GetMessageCaptureSessions(const SessionQuery& query) = 0;
	virtual uint64_t CountMessageCaptureSessions(const SessionQuery& query) = 0;

	// A session's messages with seq above afterSeq, in order
	virtual std::vector<MessageCaptureRecord> GetMessageCaptureEntries(uint64_t sessionId, uint32_t afterSeq, uint32_t limit) = 0;

	// The session and its messages
	virtual void DeleteMessageCaptureSession(uint64_t id) = 0;
};

#endif  //!__IMESSAGECAPTURES__H__
