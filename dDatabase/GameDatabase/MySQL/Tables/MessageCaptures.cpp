#include "MySQLDatabase.h"

#include <sstream>

// The session filters bind the same parameters whether they are on or off (as in ChatLog.cpp), so the list and the
// count take one argument list
namespace {
	std::string Where(const IMessageCaptures::SessionQuery& q) {
		std::string where = " WHERE 1 = 1";
		where += q.characterId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND character_id = ?)";
		where += q.accountId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND account_id = ?)";
		where += q.startedBy.empty() ? " AND (? = '' AND ? = '')" : " AND (? <> '' AND LOWER(started_by) = LOWER(?))";
		where += q.since == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND started_at >= ?)";
		where += q.until == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND started_at < ?)";
		where += q.unfinishedOnly ? " AND (? <> 0 AND ended_at = 0)" : " AND (? = 0)";
		return where;
	}

	std::string OrderBy(const IMessageCaptures::SessionQuery& q) {
		using eOrder = IMessageCaptures::eSessionOrder;
		const std::string direction = q.ascending ? " ASC" : " DESC";
		std::string column = "started_at";
		switch (q.order) {
		case eOrder::CHARACTER: column = "character_name"; break;
		case eOrder::STARTED_BY: column = "started_by"; break;
		case eOrder::MESSAGES: column = "message_count"; break;
		case eOrder::BYTES: column = "byte_count"; break;
		default: break;
		}
		// Ties in id order, the same on every database
		return " ORDER BY " + column + direction + ", id" + direction;
	}

	IMessageCaptures::MessageCaptureSession Session(PreparedStmtResultSet& r) {
		IMessageCaptures::MessageCaptureSession s;
		s.id = r->getUInt64("id");
		s.characterId = r->getInt64("character_id");
		s.characterName = r->getString("character_name").c_str();
		s.accountId = r->getUInt("account_id");
		s.accountName = r->getString("account_name").c_str();
		s.startedById = r->getUInt("started_by_id");
		s.startedBy = r->getString("started_by").c_str();
		s.startedAt = r->getInt64("started_at");
		s.endsAt = r->getInt64("ends_at");
		s.endedAt = r->getInt64("ended_at");
		s.endReason = r->getString("end_reason").c_str();
		s.toServer = r->getInt("to_server") != 0;
		s.toClient = r->getInt("to_client") != 0;
		s.onlyMessages = r->getString("only_messages").c_str();
		s.skipMessages = r->getString("skip_messages").c_str();
		s.zoneId = r->getUInt("zone_id");
		s.instanceId = r->getUInt("instance_id");
		s.cloneId = r->getUInt("clone_id");
		s.zones = r->getString("zones").c_str();
		s.messageCount = r->getUInt64("message_count");
		s.byteCount = r->getUInt64("byte_count");
		s.dropped = r->getUInt64("dropped");
		return s;
	}
}

uint64_t MySQLDatabase::InsertMessageCaptureSession(const MessageCaptureSession& s) {
	ExecuteInsert("INSERT INTO message_capture_sessions (character_id, character_name, account_id, account_name, started_by_id, started_by, started_at, ends_at, "
		"ended_at, end_reason, to_server, to_client, only_messages, skip_messages, zone_id, instance_id, clone_id, zones, message_count, byte_count, dropped) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		s.characterId, s.characterName, s.accountId, s.accountName, s.startedById, s.startedBy, s.startedAt, s.endsAt, s.endedAt, s.endReason,
		s.toServer, s.toClient, s.onlyMessages, s.skipMessages, s.zoneId, s.instanceId, s.cloneId, s.zones, s.messageCount, s.byteCount, s.dropped);
	auto last = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;"); // this connection's insert
	return last->next() ? last->getUInt64("id") : 0;
}

void MySQLDatabase::UpdateMessageCaptureSession(const MessageCaptureSession& s) {
	ExecuteUpdate("UPDATE message_capture_sessions SET ends_at = ?, ended_at = ?, end_reason = ?, zone_id = ?, instance_id = ?, clone_id = ?, zones = ?, "
		"message_count = ?, byte_count = ?, dropped = ? WHERE id = ?;",
		s.endsAt, s.endedAt, s.endReason, s.zoneId, s.instanceId, s.cloneId, s.zones, s.messageCount, s.byteCount, s.dropped, s.id);
}

void MySQLDatabase::InsertMessageCaptureEntries(const std::vector<MessageCaptureRecord>& entries) {
	if (entries.empty()) return;
	DatabaseTransaction transaction(*this);
	for (const auto& e : entries) {
		std::istringstream payload(e.payload);
		ExecuteInsert("INSERT INTO message_capture_entries (session_id, seq, time_ms, direction, message_id, object_id, bits, dropped_before, zone_id, instance_id, "
			"clone_id, payload, decoded) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
			e.sessionId, e.seq, e.timeMs, static_cast<uint32_t>(e.direction), static_cast<uint32_t>(e.messageId), e.objectId, e.bits, e.droppedBefore, e.zoneId, e.instanceId, e.cloneId,
			static_cast<const std::istream*>(&payload), e.decoded);
	}
	transaction.Commit();
}

std::optional<IMessageCaptures::MessageCaptureSession> MySQLDatabase::GetMessageCaptureSession(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM message_capture_sessions WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return Session(result);
}

std::vector<IMessageCaptures::MessageCaptureSession> MySQLDatabase::GetMessageCaptureSessions(const SessionQuery& q) {
	std::vector<MessageCaptureSession> sessions;
	auto result = ExecuteSelect("SELECT * FROM message_capture_sessions" + Where(q) + OrderBy(q) + " LIMIT ? OFFSET ?;",
		q.characterId, q.characterId, q.accountId, q.accountId, q.startedBy, q.startedBy, q.since, q.since, q.until, q.until, q.unfinishedOnly, q.limit, q.offset);
	while (result->next()) sessions.push_back(Session(result));
	return sessions;
}

uint64_t MySQLDatabase::CountMessageCaptureSessions(const SessionQuery& q) {
	auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM message_capture_sessions" + Where(q) + ";",
		q.characterId, q.characterId, q.accountId, q.accountId, q.startedBy, q.startedBy, q.since, q.since, q.until, q.until, q.unfinishedOnly);
	return result->next() ? result->getUInt64("count") : 0;
}

std::vector<IMessageCaptures::MessageCaptureRecord> MySQLDatabase::GetMessageCaptureEntries(uint64_t sessionId, uint32_t afterSeq, uint32_t limit) {
	std::vector<MessageCaptureRecord> entries;
	auto result = ExecuteSelect("SELECT * FROM message_capture_entries WHERE session_id = ? AND seq > ? ORDER BY seq LIMIT ?;", sessionId, afterSeq, limit);
	while (result->next()) {
		MessageCaptureRecord e;
		e.sessionId = result->getUInt64("session_id");
		e.seq = result->getUInt("seq");
		e.timeMs = result->getInt64("time_ms");
		e.direction = static_cast<uint8_t>(result->getUInt("direction"));
		e.messageId = static_cast<uint16_t>(result->getUInt("message_id"));
		e.objectId = result->getInt64("object_id");
		e.bits = result->getUInt("bits");
		e.droppedBefore = result->getUInt("dropped_before");
		e.zoneId = result->getUInt("zone_id");
		e.instanceId = result->getUInt("instance_id");
		e.cloneId = result->getUInt("clone_id");
		std::unique_ptr<std::istream> blob(result->getBlob("payload"));
		std::stringstream data;
		if (blob) data << blob->rdbuf();
		e.payload = data.str();
		e.decoded = result->getString("decoded").c_str();
		entries.push_back(std::move(e));
	}
	return entries;
}

void MySQLDatabase::DeleteMessageCaptureSession(uint64_t id) {
	ExecuteDelete("DELETE FROM message_capture_entries WHERE session_id = ?;", id);
	ExecuteDelete("DELETE FROM message_capture_sessions WHERE id = ?;", id);
}
