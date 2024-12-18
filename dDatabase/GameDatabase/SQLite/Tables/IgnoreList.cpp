#include "SQLiteDatabase.h"

std::vector<IIgnoreList::Info> SQLiteDatabase::GetIgnoreList(const uint32_t playerId) {
	auto [_, result] = ExecuteSelect("SELECT ci.name AS name, il.ignored_player_id AS ignore_id FROM ignore_list AS il JOIN charinfo AS ci ON il.ignored_player_id = ci.id WHERE il.player_id = ?", playerId);

	std::vector<IIgnoreList::Info> ignoreList;

	while (!result.eof()) {
		ignoreList.push_back(IIgnoreList::Info{ result.getStringField("name"), static_cast<uint32_t>(result.getIntField("ignore_id")) });
		result.nextRow();
	}

	return ignoreList;
}

void SQLiteDatabase::AddIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) {
	ExecuteInsert("INSERT OR IGNORE INTO ignore_list (player_id, ignored_player_id) VALUES (?, ?)", playerId, ignoredPlayerId);
}

void SQLiteDatabase::RemoveIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) {
	ExecuteDelete("DELETE FROM ignore_list WHERE player_id = ? AND ignored_player_id = ?", playerId, ignoredPlayerId);
}
