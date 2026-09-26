#include "SQLiteDatabase.h"
#include "GeneralUtils.h"

// The filters are all optional. Each binds the same parameters whether it is on or off, so both queries below take one
// argument list. A filter that is off only compares its own parameters (worked out once, not per row); one that is on is
// a plain comparison, so the character and account filters are answered from chat_log's indexes instead of a full scan.
namespace {
	std::string Where(const IChatLog::ChatQuery& q) {
		std::string where = " WHERE id > ?";
		where += q.channel.empty() ? " AND (? = '' AND ? = '')" : " AND (? <> '' AND channel = ?)";
		where += q.includePrivate ? " AND (? = 1)" : " AND (? = 1 OR channel IN ('zone', 'web'))";
		where += q.characterId == 0 ? " AND (? = 0 AND ? = 0 AND ? = 0)" : " AND (? <> 0 AND (sender_id = ? OR recipient_id = ?))";
		where += q.accountId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND account_id = ?)";
		where += q.zoneId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND zone_id = ?)";
		where += q.instanceId < 0 ? " AND (? < 0 AND ? < 0)" : " AND (? >= 0 AND instance_id = ?)";
		where += q.since == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND time >= ?)";
		where += q.search.empty() ? " AND (? = '' AND ? = '' AND ? = '' AND ? = '')"
			: " AND (? <> '' AND (message LIKE '%' || ? || '%' ESCAPE '!' OR sender_name LIKE '%' || ? || '%' ESCAPE '!' OR recipient_name LIKE '%' || ? || '%' ESCAPE '!'))";
		where += q.blockedOnly ? " AND (? <> 0 AND blocked = 1)" : " AND (? = 0)";
		return where;
	}
}

uint64_t SQLiteDatabase::InsertChatMessage(const ChatMessage& m) {
	ExecuteInsert("INSERT INTO chat_log (time, channel, sender_id, sender_name, account_id, recipient_id, recipient_name, zone_id, instance_id, clone_id, message, blocked) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		m.time, m.channel, m.senderId, m.senderName, m.accountId, m.recipientId, m.recipientName, m.zoneId, m.instanceId, m.cloneId, m.message, m.blocked);
	auto [_, last] = ExecuteSelect("SELECT last_insert_rowid() AS id;"); // this connection's insert, not another server's
	return last.eof() ? 0 : static_cast<uint64_t>(last.getInt64Field("id"));
}

std::vector<IChatLog::ChatMessage> SQLiteDatabase::GetChatMessages(const ChatQuery& q) {
	std::vector<ChatMessage> messages;
	auto [_, result] = ExecuteSelect("SELECT * FROM chat_log" + Where(q) + " ORDER BY id " + std::string(q.newestFirst ? "DESC" : "ASC") + " LIMIT ? OFFSET ?;", static_cast<int64_t>(q.afterId), q.channel, q.channel, q.includePrivate, q.characterId, q.characterId, q.characterId, q.accountId, q.accountId, q.zoneId, q.zoneId, q.instanceId, q.instanceId, q.since, q.since, q.search, GeneralUtils::LikeEscape(q.search, '!'), GeneralUtils::LikeEscape(q.search, '!'), GeneralUtils::LikeEscape(q.search, '!'), q.blockedOnly, q.limit, q.offset);
	for (; !result.eof(); result.nextRow()) {
		messages.push_back({ static_cast<uint64_t>(result.getInt64Field("id")), result.getInt64Field("time"), result.getStringField("channel"),
			result.getInt64Field("sender_id"), result.getStringField("sender_name"), static_cast<uint32_t>(result.getIntField("account_id")),
			result.getInt64Field("recipient_id"), result.getStringField("recipient_name"), static_cast<uint32_t>(result.getIntField("zone_id")),
			static_cast<uint32_t>(result.getIntField("instance_id")), static_cast<uint32_t>(result.getIntField("clone_id")), result.getStringField("message"),
			result.getIntField("blocked") != 0 });
	}
	return messages;
}

uint64_t SQLiteDatabase::CountChatMessages(const ChatQuery& q) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM chat_log" + Where(q) + ";", static_cast<int64_t>(q.afterId), q.channel, q.channel, q.includePrivate, q.characterId, q.characterId, q.characterId, q.accountId, q.accountId, q.zoneId, q.zoneId, q.instanceId, q.instanceId, q.since, q.since, q.search, GeneralUtils::LikeEscape(q.search, '!'), GeneralUtils::LikeEscape(q.search, '!'), GeneralUtils::LikeEscape(q.search, '!'), q.blockedOnly);
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("count"));
}
