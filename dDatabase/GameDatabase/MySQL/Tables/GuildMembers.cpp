#include "MySQLDatabase.h"

void MySQLDatabase::InsertGuildMember(const uint32_t guild_id, const uint32_t member_id, const uint16_t rank){
	ExecuteInsert("INSERT INTO guild_members (guild_id, character_id, rank) VALUES (?, ?, ?);", guild_id, member_id, rank);
}

void MySQLDatabase::DeleteGuildMember(const uint32_t member_id){
	ExecuteDelete("DELETE FROM guild_members WHERE character_id = ?;", member_id);
}

uint32_t MySQLDatabase::GetMembersGuild(const uint32_t member_id){
	auto res = ExecuteSelect("SELECT * FROM guild_members WHERE character_id = ?;", member_id);
	if (!res->next()) return 0;
	return res->getUInt("guild_id");
}

std::vector<IGuildMembers::GuildMember> MySQLDatabase::GetGuildMembers(const uint32_t guild_id){
	auto res = ExecuteSelect("SELECT * FROM guild_members WHERE guild_id = ?;", guild_id);
	std::vector<GuildMember> toReturn;
	toReturn.reserve(res->rowsCount());
	
	while (res->next()) {
		GuildMember member;
		member.id = res->getUInt("character_id");
		member.rank = res->getUInt("rank");
		toReturn.push_back(member);
	}
	return toReturn;
}

bool MySQLDatabase::CheckIsInGuild(const uint32_t guild_id, const uint32_t character_id) {
	auto res = ExecuteSelect("SELECT * FROM guild_members WHERE guild_id = ? AND character_id = ?;", guild_id, character_id);
	if (res->next()) return true;
	return false;
}
