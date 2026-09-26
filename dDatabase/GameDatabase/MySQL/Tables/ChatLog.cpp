#include "MySQLDatabase.h"
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
			: " AND (? <> '' AND (message LIKE CONCAT('%', ?, '%') ESCAPE '!' OR sender_name LIKE CONCAT('%', ?, '%') ESCAPE '!' OR recipient_name LIKE CONCAT('%', ?, '%') ESCAPE '!'))";
		where += q.blockedOnly ? " AND (? <> 0 AND blocked = 1)" : " AND (? = 0)";
		return where;
	}
}

uint64_t MySQLDatabase::InsertChatMessage(const ChatMessage& m) {
	ExecuteInsert("INSERT INTO chat_log (time, channel, sender_id, sender_name, account_id, recipient_id, recipient_name, zone_id, instance_id, clone_id, message, blocked) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		m.time, m.channel, m.senderId, m.senderName, m.accountId, m.recipientId, m.recipientName, m.zoneId, m.instanceId, m.cloneId, m.message, m.blocked);
	auto last = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;"); // this connection's insert, not another server's
	return last->next() ? last->getUInt64("id") : 0;
}

std::vector<IChatLog::ChatMessage> MySQLDatabase::GetChatMessages(const ChatQuery& q) {
	std::vector<ChatMessage> messages;
	auto result = ExecuteSelect("SELECT * FROM chat_log" + Where(q) + " ORDER BY id " + std::string(q.newestFirst ? "DESC" : "ASC") + " LIMIT ? OFFSET ?;", static_cast<int64_t>(q.afterId), q.channel, q.channel, q.includePrivate, q.characterId, q.characterId, q.characterId, q.accountId, q.accountId, q.zoneId, q.zoneId, q.instanceId, q.instanceId, q.since, q.since, q.search, GeneralUtils::LikeEscape(q.search, '!'), GeneralUtils::LikeEscape(q.search, '!'), GeneralUtils::LikeEscape(q.search, '!'), q.blockedOnly, q.limit, q.offset);
	while (result->next()) {
		messages.push_back({ result->getUInt64("id"), result->getInt64("time"), std::string(result->getString("channel").c_str()),
			result->getInt64("sender_id"), std::string(result->getString("sender_name").c_str()), result->getUInt("account_id"),
			result->getInt64("recipient_id"), std::string(result->getString("recipient_name").c_str()), result->getUInt("zone_id"),
			result->getUInt("instance_id"), result->getUInt("clone_id"), std::string(result->getString("message").c_str()),
			result->getInt("blocked") != 0 });
	}
	return messages;
}

uint64_t MySQLDatabase::CountChatMessages(const ChatQuery& q) {
	auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM chat_log" + Where(q) + ";", static_cast<int64_t>(q.afterId), q.channel, q.channel, q.includePrivate, q.characterId, q.characterId, q.characterId, q.accountId, q.accountId, q.zoneId, q.zoneId, q.instanceId, q.instanceId, q.since, q.since, q.search, GeneralUtils::LikeEscape(q.search, '!'), GeneralUtils::LikeEscape(q.search, '!'), GeneralUtils::LikeEscape(q.search, '!'), q.blockedOnly);
	return result->next() ? result->getUInt64("count") : 0;
}
