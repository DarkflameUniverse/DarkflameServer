#include "MySQLDatabase.h"

std::optional<IGuilds::Guild> MySQLDatabase::CreateGuild(const std::string_view name, const int32_t owner_id, const uint64_t reputation) {
	ExecuteInsert("INSERT INTO guilds (name, owner_id, reputation) VALUES (?, ?, ?);", name, owner_id, reputation);
	return GetGuildByName(name);
}

std::optional<IGuilds::Guild> MySQLDatabase::GetGuild(const uint32_t guild_id) {
	auto return_res = ExecuteSelect("SELECT * from guilds where id = ?", guild_id);
	if (!return_res->next()) return std::nullopt;

	IGuilds::Guild toReturn;
	toReturn.id = return_res->getInt64("id");
	toReturn.owner_id = return_res->getUInt("owner_id");
	toReturn.name = return_res->getString("name").c_str();
	toReturn.motd = return_res->getString("motd").c_str();
	toReturn.reputation = return_res->getUInt64("reputation");

	return toReturn;
}
std::optional<IGuilds::Guild> MySQLDatabase::GetGuildByName(const std::string_view name) {
	auto return_res = ExecuteSelect("SELECT * from guilds where name = ?", name);
	if (!return_res->next()) return std::nullopt;
	
	IGuilds::Guild toReturn;
	toReturn.id = return_res->getUInt("id");
	toReturn.owner_id = return_res->getUInt("owner_id");
	toReturn.name = return_res->getString("name").c_str();
	toReturn.motd = return_res->getString("motd").c_str();
	toReturn.reputation = return_res->getUInt64("reputation");

	return toReturn;
}

bool MySQLDatabase::CheckGuildNameExists(const std::string_view name) {
	auto res = ExecuteSelect("SELECT * FROM guilds WHERE name = ?;", name);
	return res->next();
}
void MySQLDatabase::SetMOTD(const uint32_t guild_id, const std::string_view motd) {
	auto res = ExecuteUpdate("Update guilds SET motd = ? WHERE id = ?;", motd, guild_id);
	if (res != 1) throw res;
}

void MySQLDatabase::DeleteGuild(const uint32_t guild_id) {
	ExecuteDelete("DELETE FROM guilds where id = ?;", guild_id);
}