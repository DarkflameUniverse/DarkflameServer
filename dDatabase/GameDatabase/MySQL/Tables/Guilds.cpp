#include "MySQLDatabase.h"

#include "GeneralUtils.h"

namespace {
	template<typename Result> IGuilds::Guild ReadGuild(Result& result) {
		IGuilds::Guild guild;
		guild.id = result->getInt64("id");
		guild.name = result->getString("name").c_str();
		guild.nameStatus = result->getInt("name_status");
		guild.founderId = result->getInt64("founder_id");
		guild.createdAt = result->getInt64("created_at");
		return guild;
	}

	template<typename Result> IGuilds::Member ReadMember(Result& result) {
		IGuilds::Member member;
		member.characterId = result->getInt64("character_id");
		member.guildId = result->getInt64("guild_id");
		member.rank = static_cast<uint8_t>(result->getUInt("guild_rank"));
		member.joinedAt = result->getInt64("joined_at");
		member.name = result->isNull("name") ? "" : result->getString("name").c_str();
		return member;
	}

	template<typename Result> IGuilds::Event ReadEvent(Result& result) {
		IGuilds::Event event;
		event.id = result->getUInt64("id");
		event.guildId = result->getInt64("guild_id");
		event.time = result->getInt64("time");
		event.kind = result->getString("kind").c_str();
		event.characterId = result->getInt64("character_id");
		event.characterName = result->getString("character_name").c_str();
		event.actor = result->getString("actor").c_str();
		event.detail = result->getString("detail").c_str();
		return event;
	}

	constexpr const char* MEMBER_SELECT = "SELECT m.character_id, m.guild_id, m.guild_rank, m.joined_at, c.name "
		"FROM guild_members m LEFT JOIN charinfo c ON c.id = m.character_id ";

	// The member count and leader (lowest rank, first to join) of the guild in the outer query as g
	constexpr const char* SUMMARY_SELECT = "SELECT g.*, "
		"(SELECT COUNT(*) FROM guild_members m WHERE m.guild_id = g.id) AS member_count, "
		"(SELECT m.character_id FROM guild_members m WHERE m.guild_id = g.id ORDER BY m.guild_rank, m.joined_at, m.character_id LIMIT 1) AS leader_id "
		"FROM guilds g";
}

int64_t MySQLDatabase::InsertGuild(const Guild& guild) {
	ExecuteInsert("INSERT INTO guilds (name, name_status, founder_id, created_at) VALUES (?, ?, ?, ?);",
		guild.name, guild.nameStatus, guild.founderId, guild.createdAt);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getInt64("id") : 0;
}

std::optional<IGuilds::Guild> MySQLDatabase::GetGuild(int64_t guildId) {
	auto result = ExecuteSelect("SELECT * FROM guilds WHERE id = ?;", guildId);
	if (!result->next()) return std::nullopt;
	return ReadGuild(result);
}

std::optional<IGuilds::Guild> MySQLDatabase::GetGuildByName(const std::string& name) {
	auto result = ExecuteSelect("SELECT * FROM guilds WHERE LOWER(name) = LOWER(?) LIMIT 1;", name);
	if (!result->next()) return std::nullopt;
	return ReadGuild(result);
}

void MySQLDatabase::SetGuildName(int64_t guildId, const std::string& name, int32_t nameStatus) {
	ExecuteUpdate("UPDATE guilds SET name = ?, name_status = ? WHERE id = ?;", name, nameStatus, guildId);
}

void MySQLDatabase::DeleteGuild(int64_t guildId) {
	ExecuteDelete("DELETE FROM guild_members WHERE guild_id = ?;", guildId);
	ExecuteDelete("DELETE FROM guild_invites WHERE guild_id = ?;", guildId);
	ExecuteDelete("DELETE FROM guilds WHERE id = ?;", guildId);
}

void MySQLDatabase::AddGuildMember(const Member& member) {
	ExecuteInsert("INSERT INTO guild_members (character_id, guild_id, guild_rank, joined_at) VALUES (?, ?, ?, ?);",
		member.characterId, member.guildId, static_cast<uint32_t>(member.rank), member.joinedAt);
}

void MySQLDatabase::RemoveGuildMember(LWOOBJID characterId) {
	ExecuteDelete("DELETE FROM guild_members WHERE character_id = ?;", characterId);
}

void MySQLDatabase::SetGuildMemberRank(LWOOBJID characterId, uint8_t rank) {
	ExecuteUpdate("UPDATE guild_members SET guild_rank = ? WHERE character_id = ?;", static_cast<uint32_t>(rank), characterId);
}

std::optional<IGuilds::Member> MySQLDatabase::GetGuildMember(LWOOBJID characterId) {
	auto result = ExecuteSelect(std::string(MEMBER_SELECT) + "WHERE m.character_id = ?;", characterId);
	if (!result->next()) return std::nullopt;
	return ReadMember(result);
}

std::vector<IGuilds::Member> MySQLDatabase::GetGuildMembers(int64_t guildId) {
	std::vector<Member> members;
	auto result = ExecuteSelect(std::string(MEMBER_SELECT) + "WHERE m.guild_id = ? ORDER BY m.guild_rank, m.joined_at, m.character_id;", guildId);
	while (result->next()) members.push_back(ReadMember(result));
	return members;
}

void MySQLDatabase::SetGuildInvite(const Invite& invite) {
	ExecuteInsert("REPLACE INTO guild_invites (character_id, guild_id, inviter_id, created_at) VALUES (?, ?, ?, ?);",
		invite.characterId, invite.guildId, invite.inviterId, invite.createdAt);
}

std::optional<IGuilds::Invite> MySQLDatabase::GetGuildInvite(LWOOBJID characterId) {
	auto result = ExecuteSelect("SELECT * FROM guild_invites WHERE character_id = ?;", characterId);
	if (!result->next()) return std::nullopt;
	Invite invite;
	invite.characterId = result->getInt64("character_id");
	invite.guildId = result->getInt64("guild_id");
	invite.inviterId = result->getInt64("inviter_id");
	invite.createdAt = result->getInt64("created_at");
	return invite;
}

void MySQLDatabase::DeleteGuildInvite(LWOOBJID characterId) {
	ExecuteDelete("DELETE FROM guild_invites WHERE character_id = ?;", characterId);
}

uint64_t MySQLDatabase::InsertGuildEvent(const Event& event) {
	ExecuteInsert("INSERT INTO guild_events (guild_id, time, kind, character_id, character_name, actor, detail) VALUES (?, ?, ?, ?, ?, ?, ?);",
		event.guildId, event.time, event.kind, event.characterId, event.characterName, event.actor, event.detail);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getUInt64("id") : 0;
}

std::vector<IGuilds::Event> MySQLDatabase::GetGuildEvents(int64_t guildId, uint32_t limit) {
	std::vector<Event> events;
	auto result = guildId == 0
		? ExecuteSelect("SELECT * FROM guild_events ORDER BY id DESC LIMIT ?;", limit)
		: ExecuteSelect("SELECT * FROM guild_events WHERE guild_id = ? ORDER BY id DESC LIMIT ?;", guildId, limit);
	while (result->next()) events.push_back(ReadEvent(result));
	return events;
}

IGuilds::GuildPage MySQLDatabase::GetGuildPage(uint32_t start, uint32_t length, const std::string& search, bool pendingOnly) {
	GuildPage page;
	const std::string pending = pendingOnly ? " WHERE g.name_status = 1" : "";
	{
		auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM guilds g" + pending + ";");
		page.total = result->next() ? result->getUInt("count") : 0;
	}

	// A number in the search box also matches the id (-1 never does)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(search).value_or(-1);
	const std::string searchFilter = "(g.name LIKE CONCAT('%', ?, '%') OR g.id = ?)";
	const std::string where = search.empty() ? pending : (pendingOnly ? pending + " AND " : std::string(" WHERE ")) + searchFilter;
	if (search.empty()) {
		page.filtered = page.total;
	} else {
		auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM guilds g" + where + ";", search, searchId);
		page.filtered = result->next() ? result->getUInt("count") : 0;
	}

	const std::string query = std::string(SUMMARY_SELECT) + where + " ORDER BY g.id DESC LIMIT ?, ?;";
	auto result = search.empty() ? ExecuteSelect(query, start, length) : ExecuteSelect(query, search, searchId, start, length);
	while (result->next()) {
		GuildSummary summary;
		summary.guild = ReadGuild(result);
		summary.memberCount = result->getUInt("member_count");
		summary.leaderId = result->isNull("leader_id") ? 0 : result->getInt64("leader_id");
		page.guilds.push_back(std::move(summary));
	}
	for (auto& summary : page.guilds) {
		if (summary.leaderId == 0) continue;
		auto name = ExecuteSelect("SELECT name FROM charinfo WHERE id = ?;", summary.leaderId);
		if (name->next()) summary.leaderName = name->getString("name").c_str();
	}
	return page;
}
