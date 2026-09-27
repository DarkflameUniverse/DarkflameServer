#include "SQLiteDatabase.h"

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
		// MySQL compares names without case; so does this
		case eOrder::CHARACTER: column = "character_name COLLATE NOCASE"; break;
		case eOrder::STARTED_BY: column = "started_by COLLATE NOCASE"; break;
		case eOrder::MESSAGES: column = "message_count"; break;
		case eOrder::BYTES: column = "byte_count"; break;
		default: break;
		}
		// Ties in id order, the same on every database
		return " ORDER BY " + column + direction + ", id" + direction;
	}

	IMessageCaptures::MessageCaptureSession Session(CppSQLite3Query& r) {
		IMessageCaptures::MessageCaptureSession s;
		s.id = static_cast<uint64_t>(r.getInt64Field("id"));
		s.characterId = r.getInt64Field("character_id");
		s.characterName = r.getStringField("character_name");
		s.accountId = static_cast<uint32_t>(r.getInt64Field("account_id"));
		s.accountName = r.getStringField("account_name");
		s.startedById = static_cast<uint32_t>(r.getInt64Field("started_by_id"));
		s.startedBy = r.getStringField("started_by");
		s.startedAt = r.getInt64Field("started_at");
		s.endsAt = r.getInt64Field("ends_at");
		s.endedAt = r.getInt64Field("ended_at");
		s.endReason = r.getStringField("end_reason");
		s.toServer = r.getIntField("to_server") != 0;
		s.toClient = r.getIntField("to_client") != 0;
		s.onlyMessages = r.getStringField("only_messages");
		s.skipMessages = r.getStringField("skip_messages");
		s.zoneId = static_cast<uint32_t>(r.getInt64Field("zone_id"));
		s.instanceId = static_cast<uint32_t>(r.getInt64Field("instance_id"));
		s.cloneId = static_cast<uint32_t>(r.getInt64Field("clone_id"));
		s.zones = r.getStringField("zones");
		s.messageCount = static_cast<uint64_t>(r.getInt64Field("message_count"));
		s.byteCount = static_cast<uint64_t>(r.getInt64Field("byte_count"));
		s.dropped = static_cast<uint64_t>(r.getInt64Field("dropped"));
		s.kind = static_cast<uint8_t>(r.getIntField("capture_kind"));
		s.target = r.getStringField("capture_target");
		return s;
	}
}

uint64_t SQLiteDatabase::InsertMessageCaptureSession(const MessageCaptureSession& s) {
	ExecuteInsert("INSERT INTO message_capture_sessions (character_id, character_name, account_id, account_name, started_by_id, started_by, started_at, ends_at, "
		"ended_at, end_reason, to_server, to_client, only_messages, skip_messages, zone_id, instance_id, clone_id, zones, message_count, byte_count, dropped, capture_kind, capture_target) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		s.characterId, s.characterName, s.accountId, s.accountName, s.startedById, s.startedBy, s.startedAt, s.endsAt, s.endedAt, s.endReason,
		s.toServer, s.toClient, s.onlyMessages, s.skipMessages, s.zoneId, s.instanceId, s.cloneId, s.zones, s.messageCount, s.byteCount, s.dropped, static_cast<uint32_t>(s.kind), s.target);
	auto [_, last] = ExecuteSelect("SELECT last_insert_rowid() AS id;"); // this connection's insert
	return last.eof() ? 0 : static_cast<uint64_t>(last.getInt64Field("id"));
}

void SQLiteDatabase::UpdateMessageCaptureSession(const MessageCaptureSession& s) {
	ExecuteUpdate("UPDATE message_capture_sessions SET ends_at = ?, ended_at = ?, end_reason = ?, zone_id = ?, instance_id = ?, clone_id = ?, zones = ?, "
		"message_count = ?, byte_count = ?, dropped = ? WHERE id = ?;",
		s.endsAt, s.endedAt, s.endReason, s.zoneId, s.instanceId, s.cloneId, s.zones, s.messageCount, s.byteCount, s.dropped, s.id);
}

void SQLiteDatabase::InsertMessageCaptureEntries(const std::vector<MessageCaptureRecord>& entries) {
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

std::optional<IMessageCaptures::MessageCaptureSession> SQLiteDatabase::GetMessageCaptureSession(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM message_capture_sessions WHERE id = ?;", id);
	if (result.eof()) return std::nullopt;
	return Session(result);
}

std::vector<IMessageCaptures::MessageCaptureSession> SQLiteDatabase::GetMessageCaptureSessions(const SessionQuery& q) {
	std::vector<MessageCaptureSession> sessions;
	auto [_, result] = ExecuteSelect("SELECT * FROM message_capture_sessions" + Where(q) + OrderBy(q) + " LIMIT ? OFFSET ?;",
		q.characterId, q.characterId, q.accountId, q.accountId, q.startedBy, q.startedBy, q.since, q.since, q.until, q.until, q.unfinishedOnly, q.limit, q.offset);
	for (; !result.eof(); result.nextRow()) sessions.push_back(Session(result));
	return sessions;
}

uint64_t SQLiteDatabase::CountMessageCaptureSessions(const SessionQuery& q) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM message_capture_sessions" + Where(q) + ";",
		q.characterId, q.characterId, q.accountId, q.accountId, q.startedBy, q.startedBy, q.since, q.since, q.until, q.until, q.unfinishedOnly);
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("count"));
}

std::vector<IMessageCaptures::MessageCaptureRecord> SQLiteDatabase::GetMessageCaptureEntries(uint64_t sessionId, uint32_t afterSeq, uint32_t limit) {
	std::vector<MessageCaptureRecord> entries;
	auto [_, result] = ExecuteSelect("SELECT * FROM message_capture_entries WHERE session_id = ? AND seq > ? ORDER BY seq LIMIT ?;", sessionId, afterSeq, limit);
	for (; !result.eof(); result.nextRow()) {
		MessageCaptureRecord e;
		e.sessionId = static_cast<uint64_t>(result.getInt64Field("session_id"));
		e.seq = static_cast<uint32_t>(result.getInt64Field("seq"));
		e.timeMs = result.getInt64Field("time_ms");
		e.direction = static_cast<uint8_t>(result.getIntField("direction"));
		e.messageId = static_cast<uint16_t>(result.getIntField("message_id"));
		e.objectId = result.getInt64Field("object_id");
		e.bits = static_cast<uint32_t>(result.getInt64Field("bits"));
		e.droppedBefore = static_cast<uint32_t>(result.getInt64Field("dropped_before"));
		e.zoneId = static_cast<uint32_t>(result.getInt64Field("zone_id"));
		e.instanceId = static_cast<uint32_t>(result.getInt64Field("instance_id"));
		e.cloneId = static_cast<uint32_t>(result.getInt64Field("clone_id"));
		int length = 0;
		const auto* blob = result.getBlobField("payload", length);
		if (blob && length > 0) e.payload.assign(reinterpret_cast<const char*>(blob), length);
		e.decoded = result.getStringField("decoded");
		entries.push_back(std::move(e));
	}
	return entries;
}

void SQLiteDatabase::DeleteMessageCaptureSession(uint64_t id) {
	ExecuteDelete("DELETE FROM message_capture_entries WHERE session_id = ?;", id);
	ExecuteDelete("DELETE FROM message_capture_sessions WHERE id = ?;", id);
}
