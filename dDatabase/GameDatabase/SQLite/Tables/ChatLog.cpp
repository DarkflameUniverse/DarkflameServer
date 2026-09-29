#include "SQLiteDatabase.h"
#include "GeneralUtils.h"

#include <algorithm>

// The filters are all optional. Each binds the same parameters whether it is on or off, so both queries below take one
// argument list. A filter that is off only compares its own parameters (worked out once, not per row); one that is on is
// a plain comparison, so the character, account, guild and team filters are answered from chat_log's indexes instead of
// a full scan.
namespace {
	std::string Where(const IChatLog::ChatQuery& q) {
		std::string where = " WHERE id > ?";
		where += q.beforeId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND id < ?)";
		where += q.channel.empty() ? " AND (? = '' AND ? = '')" : " AND (? <> '' AND channel = ?)";
		// Zone and web chat for everyone who may read chat; team and guild chat, and whispers, each behind their own permission
		where += " AND (channel IN ('zone', 'web') OR (? = 1 AND channel IN ('team', 'guild')) OR (? = 1 AND channel = 'whisper'))";
		where += q.characterId == 0 ? " AND (? = 0 AND ? = 0 AND ? = 0)" : " AND (? <> 0 AND (sender_id = ? OR recipient_id = ?))";
		where += q.otherCharacterId == 0 ? " AND (? = 0 AND ? = 0 AND ? = 0)" : " AND (? <> 0 AND (sender_id = ? OR recipient_id = ?))";
		where += q.accountId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND account_id = ?)";
		where += q.zoneId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND zone_id = ?)";
		where += q.instanceId < 0 ? " AND (? < 0 AND ? < 0)" : " AND (? >= 0 AND instance_id = ?)";
		where += q.since == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND time >= ?)";
		where += q.until == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND time < ?)";
		where += q.guildId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND guild_id = ?)";
		where += q.teamId == 0 ? " AND (? = 0 AND ? = 0)" : " AND (? <> 0 AND team_id = ?)";
		where += q.search.empty() ? " AND (? = '' AND ? = '' AND ? = '' AND ? = '')"
			: " AND (? <> '' AND (message LIKE '%' || ? || '%' ESCAPE '!' OR sender_name LIKE '%' || ? || '%' ESCAPE '!' OR recipient_name LIKE '%' || ? || '%' ESCAPE '!'))";
		where += q.blockedOnly ? " AND (? <> 0 AND blocked = 1)" : " AND (? = 0)";
		return where;
	}

	IChatLog::ChatMessage Row(CppSQLite3Query& result) {
		IChatLog::ChatMessage m{ static_cast<uint64_t>(result.getInt64Field("id")), result.getInt64Field("time"), result.getStringField("channel"),
			result.getInt64Field("sender_id"), result.getStringField("sender_name"), static_cast<uint32_t>(result.getIntField("account_id")),
			result.getInt64Field("recipient_id"), result.getStringField("recipient_name"), static_cast<uint32_t>(result.getIntField("zone_id")),
			static_cast<uint32_t>(result.getIntField("instance_id")), static_cast<uint32_t>(result.getIntField("clone_id")), result.getStringField("message"),
			result.getIntField("blocked") != 0 };
		m.guildId = result.getInt64Field("guild_id");
		m.teamId = result.getInt64Field("team_id");
		m.filtered = result.getIntField("filtered") != 0;
		return m;
	}

	// GROUP_CONCAT lists names in whatever order the database likes; sorted, so both databases agree
	std::string SortedNames(const std::string& concatenated) {
		auto names = GeneralUtils::SplitString(concatenated, ',');
		std::ranges::sort(names);
		std::string out;
		for (const auto& name : names) {
			if (name.empty()) continue;
			out += (out.empty() ? "" : ",") + name;
		}
		return out;
	}
}

// Every query ending in Where(q) takes the filter's parameters in this order, then its own
#define CHAT_QUERY_PARAMS(q, search) static_cast<int64_t>((q).afterId), static_cast<int64_t>((q).beforeId), static_cast<int64_t>((q).beforeId), (q).channel, (q).channel, \
	(q).includePrivate, (q).includeWhispers, (q).characterId, (q).characterId, (q).characterId, (q).otherCharacterId, (q).otherCharacterId, (q).otherCharacterId, \
	(q).accountId, (q).accountId, (q).zoneId, (q).zoneId, (q).instanceId, (q).instanceId, (q).since, (q).since, (q).until, (q).until, (q).guildId, (q).guildId, \
	(q).teamId, (q).teamId, (q).search, search, search, search, (q).blockedOnly

uint64_t SQLiteDatabase::InsertChatMessage(const ChatMessage& m) {
	ExecuteInsert("INSERT INTO chat_log (time, channel, sender_id, sender_name, account_id, recipient_id, recipient_name, zone_id, instance_id, clone_id, message, blocked, guild_id, team_id, filtered) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		m.time, m.channel, m.senderId, m.senderName, m.accountId, m.recipientId, m.recipientName, m.zoneId, m.instanceId, m.cloneId, m.message, m.blocked,
		m.guildId, m.teamId, m.filtered || m.blocked);
	auto [_, last] = ExecuteSelect("SELECT last_insert_rowid() AS id;"); // this connection's insert, not another server's
	return last.eof() ? 0 : static_cast<uint64_t>(last.getInt64Field("id"));
}

std::vector<IChatLog::ChatMessage> SQLiteDatabase::GetChatMessages(const ChatQuery& q) {
	std::vector<ChatMessage> messages;
	const auto search = GeneralUtils::LikeEscape(q.search, '!');
	auto [_, result] = ExecuteSelect("SELECT * FROM chat_log" + Where(q) + " ORDER BY id " + std::string(q.newestFirst ? "DESC" : "ASC") + " LIMIT ? OFFSET ?;",
		CHAT_QUERY_PARAMS(q, search), q.limit, q.offset);
	for (; !result.eof(); result.nextRow()) messages.push_back(Row(result));
	return messages;
}

uint64_t SQLiteDatabase::CountChatMessages(const ChatQuery& q) {
	const auto search = GeneralUtils::LikeEscape(q.search, '!');
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM chat_log" + Where(q) + ";", CHAT_QUERY_PARAMS(q, search));
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("count"));
}

std::vector<IChatLog::WhisperPartner> SQLiteDatabase::GetWhisperPartners(const LWOOBJID characterId, const uint32_t offset, const uint32_t limit) {
	std::vector<WhisperPartner> partners;
	auto [_, result] = ExecuteSelect(
		"SELECT g.partner, g.messages, g.first_time, g.last_time, CASE WHEN l.sender_id = g.partner THEN l.sender_name ELSE l.recipient_name END AS name FROM "
		"(SELECT CASE WHEN sender_id = ? THEN recipient_id ELSE sender_id END AS partner, COUNT(*) AS messages, MIN(time) AS first_time, MAX(time) AS last_time, MAX(id) AS last_id "
		"FROM chat_log WHERE channel = 'whisper' AND (sender_id = ? OR recipient_id = ?) GROUP BY partner) g "
		"JOIN chat_log l ON l.id = g.last_id ORDER BY g.last_id DESC LIMIT ? OFFSET ?;", characterId, characterId, characterId, limit, offset);
	for (; !result.eof(); result.nextRow()) {
		partners.push_back({ result.getInt64Field("partner"), result.getStringField("name"), static_cast<uint64_t>(result.getInt64Field("messages")),
			result.getInt64Field("first_time"), result.getInt64Field("last_time") });
	}
	return partners;
}

uint64_t SQLiteDatabase::CountWhisperPartners(const LWOOBJID characterId) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(DISTINCT CASE WHEN sender_id = ? THEN recipient_id ELSE sender_id END) AS count FROM chat_log "
		"WHERE channel = 'whisper' AND (sender_id = ? OR recipient_id = ?);", characterId, characterId, characterId);
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("count"));
}

std::vector<IChatLog::ChatTeam> SQLiteDatabase::GetChatTeams(const LWOOBJID characterId, const uint32_t offset, const uint32_t limit) {
	std::vector<ChatTeam> teams;
	auto [_, result] = ExecuteSelect("SELECT team_id, COUNT(*) AS messages, MIN(time) AS first_time, MAX(time) AS last_time, MAX(id) AS last_id, GROUP_CONCAT(DISTINCT sender_name) AS senders "
		"FROM chat_log WHERE channel = 'team' AND team_id <> 0 AND (? = 0 OR team_id IN (SELECT team_id FROM chat_log WHERE channel = 'team' AND sender_id = ?)) "
		"GROUP BY team_id ORDER BY last_id DESC LIMIT ? OFFSET ?;", characterId, characterId, limit, offset);
	for (; !result.eof(); result.nextRow()) {
		teams.push_back({ result.getInt64Field("team_id"), static_cast<uint64_t>(result.getInt64Field("messages")), result.getInt64Field("first_time"),
			result.getInt64Field("last_time"), SortedNames(result.getStringField("senders")) });
	}
	return teams;
}

uint64_t SQLiteDatabase::CountChatTeams(const LWOOBJID characterId) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(DISTINCT team_id) AS count FROM chat_log WHERE channel = 'team' AND team_id <> 0 "
		"AND (? = 0 OR team_id IN (SELECT team_id FROM chat_log WHERE channel = 'team' AND sender_id = ?));", characterId, characterId);
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("count"));
}
