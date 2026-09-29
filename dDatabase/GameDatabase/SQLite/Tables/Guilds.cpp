#include "SQLiteDatabase.h"

#include "GeneralUtils.h"

namespace {
	IGuilds::Guild ReadGuild(CppSQLite3Query& result) {
		IGuilds::Guild guild;
		guild.id = result.getInt64Field("id");
		guild.name = result.getStringField("name");
		guild.nameStatus = result.getIntField("name_status");
		guild.founderId = result.getInt64Field("founder_id");
		guild.createdAt = result.getInt64Field("created_at");
		return guild;
	}

	IGuilds::Member ReadMember(CppSQLite3Query& result) {
		IGuilds::Member member;
		member.characterId = result.getInt64Field("character_id");
		member.guildId = result.getInt64Field("guild_id");
		member.rank = static_cast<uint8_t>(result.getIntField("guild_rank"));
		member.joinedAt = result.getInt64Field("joined_at");
		member.name = result.getStringField("name", "");
		return member;
	}

	IGuilds::Event ReadEvent(CppSQLite3Query& result) {
		IGuilds::Event event;
		event.id = static_cast<uint64_t>(result.getInt64Field("id"));
		event.guildId = result.getInt64Field("guild_id");
		event.time = result.getInt64Field("time");
		event.kind = result.getStringField("kind");
		event.characterId = result.getInt64Field("character_id");
		event.characterName = result.getStringField("character_name");
		event.actor = result.getStringField("actor");
		event.detail = result.getStringField("detail");
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

int64_t SQLiteDatabase::InsertGuild(const Guild& guild) {
	ExecuteInsert("INSERT INTO guilds (name, name_status, founder_id, created_at) VALUES (?, ?, ?, ?);",
		guild.name, guild.nameStatus, guild.founderId, guild.createdAt);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : result.getInt64Field("id");
}

std::optional<IGuilds::Guild> SQLiteDatabase::GetGuild(int64_t guildId) {
	auto [_, result] = ExecuteSelect("SELECT * FROM guilds WHERE id = ?;", guildId);
	if (result.eof()) return std::nullopt;
	return ReadGuild(result);
}

std::optional<IGuilds::Guild> SQLiteDatabase::GetGuildByName(const std::string& name) {
	auto [_, result] = ExecuteSelect("SELECT * FROM guilds WHERE LOWER(name) = LOWER(?) LIMIT 1;", name);
	if (result.eof()) return std::nullopt;
	return ReadGuild(result);
}

void SQLiteDatabase::SetGuildName(int64_t guildId, const std::string& name, int32_t nameStatus) {
	ExecuteUpdate("UPDATE guilds SET name = ?, name_status = ? WHERE id = ?;", name, nameStatus, guildId);
}

void SQLiteDatabase::DeleteGuild(int64_t guildId) {
	ExecuteDelete("DELETE FROM guild_members WHERE guild_id = ?;", guildId);
	ExecuteDelete("DELETE FROM guild_invites WHERE guild_id = ?;", guildId);
	ExecuteDelete("DELETE FROM guilds WHERE id = ?;", guildId);
}

void SQLiteDatabase::AddGuildMember(const Member& member) {
	ExecuteInsert("INSERT INTO guild_members (character_id, guild_id, guild_rank, joined_at) VALUES (?, ?, ?, ?);",
		member.characterId, member.guildId, static_cast<uint32_t>(member.rank), member.joinedAt);
}

void SQLiteDatabase::RemoveGuildMember(LWOOBJID characterId) {
	ExecuteDelete("DELETE FROM guild_members WHERE character_id = ?;", characterId);
}

void SQLiteDatabase::SetGuildMemberRank(LWOOBJID characterId, uint8_t rank) {
	ExecuteUpdate("UPDATE guild_members SET guild_rank = ? WHERE character_id = ?;", static_cast<uint32_t>(rank), characterId);
}

std::optional<IGuilds::Member> SQLiteDatabase::GetGuildMember(LWOOBJID characterId) {
	auto [_, result] = ExecuteSelect(std::string(MEMBER_SELECT) + "WHERE m.character_id = ?;", characterId);
	if (result.eof()) return std::nullopt;
	return ReadMember(result);
}

std::vector<IGuilds::Member> SQLiteDatabase::GetGuildMembers(int64_t guildId) {
	std::vector<Member> members;
	auto [_, result] = ExecuteSelect(std::string(MEMBER_SELECT) + "WHERE m.guild_id = ? ORDER BY m.guild_rank, m.joined_at, m.character_id;", guildId);
	for (; !result.eof(); result.nextRow()) members.push_back(ReadMember(result));
	return members;
}

void SQLiteDatabase::SetGuildInvite(const Invite& invite) {
	ExecuteInsert("REPLACE INTO guild_invites (character_id, guild_id, inviter_id, created_at) VALUES (?, ?, ?, ?);",
		invite.characterId, invite.guildId, invite.inviterId, invite.createdAt);
}

std::optional<IGuilds::Invite> SQLiteDatabase::GetGuildInvite(LWOOBJID characterId) {
	auto [_, result] = ExecuteSelect("SELECT * FROM guild_invites WHERE character_id = ?;", characterId);
	if (result.eof()) return std::nullopt;
	Invite invite;
	invite.characterId = result.getInt64Field("character_id");
	invite.guildId = result.getInt64Field("guild_id");
	invite.inviterId = result.getInt64Field("inviter_id");
	invite.createdAt = result.getInt64Field("created_at");
	return invite;
}

void SQLiteDatabase::DeleteGuildInvite(LWOOBJID characterId) {
	ExecuteDelete("DELETE FROM guild_invites WHERE character_id = ?;", characterId);
}

uint64_t SQLiteDatabase::InsertGuildEvent(const Event& event) {
	ExecuteInsert("INSERT INTO guild_events (guild_id, time, kind, character_id, character_name, actor, detail) VALUES (?, ?, ?, ?, ?, ?, ?);",
		event.guildId, event.time, event.kind, event.characterId, event.characterName, event.actor, event.detail);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("id"));
}

std::vector<IGuilds::Event> SQLiteDatabase::GetGuildEvents(int64_t guildId, uint32_t limit) {
	std::vector<Event> events;
	auto [_, result] = guildId == 0
		? ExecuteSelect("SELECT * FROM guild_events ORDER BY id DESC LIMIT ?;", limit)
		: ExecuteSelect("SELECT * FROM guild_events WHERE guild_id = ? ORDER BY id DESC LIMIT ?;", guildId, limit);
	for (; !result.eof(); result.nextRow()) events.push_back(ReadEvent(result));
	return events;
}

IGuilds::GuildPage SQLiteDatabase::GetGuildPage(uint32_t start, uint32_t length, const std::string& search, bool pendingOnly) {
	GuildPage page;
	const std::string pending = pendingOnly ? " WHERE g.name_status = 1" : "";
	{
		auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM guilds g" + pending + ";");
		page.total = result.eof() ? 0 : static_cast<uint32_t>(result.getIntField("count"));
	}

	// A number in the search box also matches the id (-1 never does)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(search).value_or(-1);
	const std::string searchFilter = "(g.name LIKE '%' || ? || '%' OR g.id = ?)";
	const std::string where = search.empty() ? pending : (pendingOnly ? pending + " AND " : std::string(" WHERE ")) + searchFilter;
	if (search.empty()) {
		page.filtered = page.total;
	} else {
		auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM guilds g" + where + ";", search, searchId);
		page.filtered = result.eof() ? 0 : static_cast<uint32_t>(result.getIntField("count"));
	}

	const std::string query = std::string(SUMMARY_SELECT) + where + " ORDER BY g.id DESC LIMIT ? OFFSET ?;";
	auto [_, result] = search.empty() ? ExecuteSelect(query, length, start) : ExecuteSelect(query, search, searchId, length, start);
	for (; !result.eof(); result.nextRow()) {
		GuildSummary summary;
		summary.guild = ReadGuild(result);
		summary.memberCount = static_cast<uint32_t>(result.getIntField("member_count"));
		summary.leaderId = result.getInt64Field("leader_id", 0);
		page.guilds.push_back(std::move(summary));
	}
	for (auto& summary : page.guilds) {
		if (summary.leaderId == 0) continue;
		auto [__, name] = ExecuteSelect("SELECT name FROM charinfo WHERE id = ?;", summary.leaderId);
		if (!name.eof()) summary.leaderName = name.getStringField("name");
	}
	return page;
}
