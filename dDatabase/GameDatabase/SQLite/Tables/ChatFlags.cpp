#include "SQLiteDatabase.h"

namespace {
	// Every column but messages (the copy of the chat, only read for one flag)
	constexpr const char* LIST_COLUMNS = "id, created_at, created_by_id, created_by, status, channel, character_id, character_name, account_id, "
		"first_message_id, last_message_id, first_time, last_time, excerpt, note, player_report_id, updated_at, updated_by";

	// Each filter binds the same parameters whether it is on or off (see ChatLog.cpp)
	std::string Where(const IChatFlags::ChatFlagQuery& q) {
		std::string where = " WHERE (channel IN ('zone', 'web') OR (? = 1 AND channel IN ('team', 'guild')) OR (? = 1 AND channel = 'whisper'))";
		where += q.status.empty() ? " AND (? = '' AND ? = '')" : " AND (? <> '' AND status = ?)";
		where += q.characterId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND character_id = ?)";
		where += q.accountId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND account_id = ?)";
		return where;
	}

	IChatFlags::ChatFlag Row(CppSQLite3Query& r, bool withMessages) {
		IChatFlags::ChatFlag f;
		f.id = static_cast<uint64_t>(r.getInt64Field("id"));
		f.createdAt = r.getInt64Field("created_at");
		f.createdById = static_cast<uint32_t>(r.getInt64Field("created_by_id"));
		f.createdBy = r.getStringField("created_by");
		f.status = r.getStringField("status");
		f.channel = r.getStringField("channel");
		f.characterId = r.getInt64Field("character_id");
		f.characterName = r.getStringField("character_name");
		f.accountId = static_cast<uint32_t>(r.getInt64Field("account_id"));
		f.firstMessageId = static_cast<uint64_t>(r.getInt64Field("first_message_id"));
		f.lastMessageId = static_cast<uint64_t>(r.getInt64Field("last_message_id"));
		f.firstTime = r.getInt64Field("first_time");
		f.lastTime = r.getInt64Field("last_time");
		f.excerpt = r.getStringField("excerpt");
		f.note = r.getStringField("note");
		if (withMessages) f.messages = r.getStringField("messages");
		f.playerReportId = static_cast<uint64_t>(r.getInt64Field("player_report_id"));
		f.updatedAt = r.getInt64Field("updated_at");
		f.updatedBy = r.getStringField("updated_by");
		return f;
	}
}

#define CHAT_FLAG_QUERY_PARAMS(q) (q).includePrivate, (q).includeWhispers, (q).status, (q).status, (q).characterId, (q).characterId, (q).accountId, (q).accountId

uint64_t SQLiteDatabase::InsertChatFlag(const ChatFlag& f, const std::vector<uint64_t>& messageIds) {
	ExecuteInsert("INSERT INTO chat_flags (created_at, created_by_id, created_by, status, channel, character_id, character_name, account_id, first_message_id, "
		"last_message_id, first_time, last_time, excerpt, note, messages, player_report_id, updated_at, updated_by) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		f.createdAt, f.createdById, f.createdBy, f.status, f.channel, f.characterId, f.characterName, f.accountId, f.firstMessageId, f.lastMessageId,
		f.firstTime, f.lastTime, f.excerpt, f.note, f.messages, f.playerReportId, f.updatedAt, f.updatedBy);
	auto [_, last] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	const uint64_t id = last.eof() ? 0 : static_cast<uint64_t>(last.getInt64Field("id"));
	for (const auto messageId : messageIds) {
		ExecuteInsert("INSERT OR IGNORE INTO chat_flag_messages (flag_id, message_id) VALUES (?, ?);", id, messageId);
	}
	return id;
}

std::optional<IChatFlags::ChatFlag> SQLiteDatabase::GetChatFlag(const uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM chat_flags WHERE id = ?;", id);
	if (result.eof()) return std::nullopt;
	return Row(result, true);
}

std::vector<IChatFlags::ChatFlag> SQLiteDatabase::GetChatFlags(const ChatFlagQuery& q) {
	std::vector<ChatFlag> flags;
	auto [_, result] = ExecuteSelect(std::string("SELECT ") + LIST_COLUMNS + " FROM chat_flags" + Where(q) + " ORDER BY id DESC LIMIT ? OFFSET ?;",
		CHAT_FLAG_QUERY_PARAMS(q), q.limit, q.offset);
	for (; !result.eof(); result.nextRow()) flags.push_back(Row(result, false));
	return flags;
}

uint64_t SQLiteDatabase::CountChatFlags(const ChatFlagQuery& q) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM chat_flags" + Where(q) + ";", CHAT_FLAG_QUERY_PARAMS(q));
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("count"));
}

bool SQLiteDatabase::UpdateChatFlag(const uint64_t id, const std::string& status, const std::string& note, const uint64_t playerReportId, const int64_t time, const std::string& updatedBy) {
	return ExecuteUpdate("UPDATE chat_flags SET status = ?, note = ?, player_report_id = ?, updated_at = ?, updated_by = ? WHERE id = ?;", status, note, playerReportId, time, updatedBy, id) > 0;
}

void SQLiteDatabase::InsertChatFlagEvent(const ChatFlagEvent& e) {
	ExecuteInsert("INSERT INTO chat_flag_events (flag_id, time, account_id, actor, action, detail) VALUES (?, ?, ?, ?, ?, ?);",
		e.flagId, e.time, e.accountId, e.actor, e.action, e.detail);
}

std::vector<IChatFlags::ChatFlagEvent> SQLiteDatabase::GetChatFlagEvents(const uint64_t flagId) {
	std::vector<ChatFlagEvent> events;
	auto [_, result] = ExecuteSelect("SELECT * FROM chat_flag_events WHERE flag_id = ? ORDER BY id ASC;", flagId);
	for (; !result.eof(); result.nextRow()) {
		events.push_back({ static_cast<uint64_t>(result.getInt64Field("id")), static_cast<uint64_t>(result.getInt64Field("flag_id")), result.getInt64Field("time"),
			static_cast<uint32_t>(result.getInt64Field("account_id")), result.getStringField("actor"), result.getStringField("action"), result.getStringField("detail") });
	}
	return events;
}

std::vector<std::pair<uint64_t, uint64_t>> SQLiteDatabase::GetFlaggedMessages(const std::vector<uint64_t>& messageIds) {
	std::vector<std::pair<uint64_t, uint64_t>> flagged;
	if (messageIds.empty()) return flagged;
	// Numbers only, written by us: safe to put in the query
	std::string list;
	for (const auto id : messageIds) list += (list.empty() ? "" : ",") + std::to_string(id);
	auto [_, result] = ExecuteSelect("SELECT message_id, MAX(flag_id) AS flag_id FROM chat_flag_messages WHERE message_id IN (" + list + ") GROUP BY message_id ORDER BY message_id;");
	for (; !result.eof(); result.nextRow()) flagged.emplace_back(static_cast<uint64_t>(result.getInt64Field("message_id")), static_cast<uint64_t>(result.getInt64Field("flag_id")));
	return flagged;
}
