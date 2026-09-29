#include "MySQLDatabase.h"

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

	IChatFlags::ChatFlag Row(sql::ResultSet& r, bool withMessages) {
		IChatFlags::ChatFlag f;
		f.id = r.getUInt64("id");
		f.createdAt = r.getInt64("created_at");
		f.createdById = r.getUInt("created_by_id");
		f.createdBy = r.getString("created_by").c_str();
		f.status = r.getString("status").c_str();
		f.channel = r.getString("channel").c_str();
		f.characterId = r.getInt64("character_id");
		f.characterName = r.getString("character_name").c_str();
		f.accountId = r.getUInt("account_id");
		f.firstMessageId = r.getUInt64("first_message_id");
		f.lastMessageId = r.getUInt64("last_message_id");
		f.firstTime = r.getInt64("first_time");
		f.lastTime = r.getInt64("last_time");
		f.excerpt = r.getString("excerpt").c_str();
		f.note = r.getString("note").c_str();
		if (withMessages) f.messages = r.getString("messages").c_str();
		f.playerReportId = r.getUInt64("player_report_id");
		f.updatedAt = r.getInt64("updated_at");
		f.updatedBy = r.getString("updated_by").c_str();
		return f;
	}
}

#define CHAT_FLAG_QUERY_PARAMS(q) (q).includePrivate, (q).includeWhispers, (q).status, (q).status, (q).characterId, (q).characterId, (q).accountId, (q).accountId

uint64_t MySQLDatabase::InsertChatFlag(const ChatFlag& f, const std::vector<uint64_t>& messageIds) {
	ExecuteInsert("INSERT INTO chat_flags (created_at, created_by_id, created_by, status, channel, character_id, character_name, account_id, first_message_id, "
		"last_message_id, first_time, last_time, excerpt, note, messages, player_report_id, updated_at, updated_by) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		f.createdAt, f.createdById, f.createdBy, f.status, f.channel, f.characterId, f.characterName, f.accountId, f.firstMessageId, f.lastMessageId,
		f.firstTime, f.lastTime, f.excerpt, f.note, f.messages, f.playerReportId, f.updatedAt, f.updatedBy);
	auto last = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	const uint64_t id = last->next() ? last->getUInt64("id") : 0;
	for (const auto messageId : messageIds) {
		ExecuteInsert("INSERT IGNORE INTO chat_flag_messages (flag_id, message_id) VALUES (?, ?);", id, messageId);
	}
	return id;
}

std::optional<IChatFlags::ChatFlag> MySQLDatabase::GetChatFlag(const uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM chat_flags WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return Row(*result.m_resultSet, true);
}

std::vector<IChatFlags::ChatFlag> MySQLDatabase::GetChatFlags(const ChatFlagQuery& q) {
	std::vector<ChatFlag> flags;
	auto result = ExecuteSelect(std::string("SELECT ") + LIST_COLUMNS + " FROM chat_flags" + Where(q) + " ORDER BY id DESC LIMIT ? OFFSET ?;",
		CHAT_FLAG_QUERY_PARAMS(q), q.limit, q.offset);
	while (result->next()) flags.push_back(Row(*result.m_resultSet, false));
	return flags;
}

uint64_t MySQLDatabase::CountChatFlags(const ChatFlagQuery& q) {
	auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM chat_flags" + Where(q) + ";", CHAT_FLAG_QUERY_PARAMS(q));
	return result->next() ? result->getUInt64("count") : 0;
}

bool MySQLDatabase::UpdateChatFlag(const uint64_t id, const std::string& status, const std::string& note, const uint64_t playerReportId, const int64_t time, const std::string& updatedBy) {
	auto exists = ExecuteSelect("SELECT id FROM chat_flags WHERE id = ?;", id);
	if (!exists->next()) return false;
	ExecuteUpdate("UPDATE chat_flags SET status = ?, note = ?, player_report_id = ?, updated_at = ?, updated_by = ? WHERE id = ?;", status, note, playerReportId, time, updatedBy, id);
	return true;
}

void MySQLDatabase::InsertChatFlagEvent(const ChatFlagEvent& e) {
	ExecuteInsert("INSERT INTO chat_flag_events (flag_id, time, account_id, actor, action, detail) VALUES (?, ?, ?, ?, ?, ?);",
		e.flagId, e.time, e.accountId, e.actor, e.action, e.detail);
}

std::vector<IChatFlags::ChatFlagEvent> MySQLDatabase::GetChatFlagEvents(const uint64_t flagId) {
	std::vector<ChatFlagEvent> events;
	auto result = ExecuteSelect("SELECT * FROM chat_flag_events WHERE flag_id = ? ORDER BY id ASC;", flagId);
	while (result->next()) {
		events.push_back({ result->getUInt64("id"), result->getUInt64("flag_id"), result->getInt64("time"), result->getUInt("account_id"),
			std::string(result->getString("actor").c_str()), std::string(result->getString("action").c_str()), std::string(result->getString("detail").c_str()) });
	}
	return events;
}

std::vector<std::pair<uint64_t, uint64_t>> MySQLDatabase::GetFlaggedMessages(const std::vector<uint64_t>& messageIds) {
	std::vector<std::pair<uint64_t, uint64_t>> flagged;
	if (messageIds.empty()) return flagged;
	// Numbers only, written by us: safe to put in the query
	std::string list;
	for (const auto id : messageIds) list += (list.empty() ? "" : ",") + std::to_string(id);
	auto result = ExecuteSelect("SELECT message_id, MAX(flag_id) AS flag_id FROM chat_flag_messages WHERE message_id IN (" + list + ") GROUP BY message_id ORDER BY message_id;");
	while (result->next()) flagged.emplace_back(result->getUInt64("message_id"), result->getUInt64("flag_id"));
	return flagged;
}
