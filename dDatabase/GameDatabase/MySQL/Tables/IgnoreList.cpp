#include "MySQLDatabase.h"

std::vector<IIgnoreList::Info> MySQLDatabase::GetIgnoreList(const uint32_t playerId) {
	auto result = ExecuteSelect("SELECT ci.name AS name, il.ignored_player_id AS ignore_id FROM ignore_list AS il JOIN charinfo AS ci ON il.ignored_player_id = ci.id WHERE il.player_id = ?", playerId);

	std::vector<IIgnoreList::Info> ignoreList;

	ignoreList.reserve(result->rowsCount());
	while (result->next()) {
		ignoreList.push_back(IIgnoreList::Info{ result->getString("name").c_str(), result->getUInt("ignore_id") });
	}

	return ignoreList;
}

void MySQLDatabase::AddIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) {
	ExecuteInsert("INSERT INTO ignore_list (player_id, ignored_player_id) VALUES (?, ?)", playerId, ignoredPlayerId);
}

void MySQLDatabase::RemoveIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) {
	ExecuteDelete("DELETE FROM ignore_list WHERE player_id = ? AND ignored_player_id = ?", playerId, ignoredPlayerId);
}
