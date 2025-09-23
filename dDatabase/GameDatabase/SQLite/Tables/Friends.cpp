#include "SQLiteDatabase.h"

std::vector<FriendData> SQLiteDatabase::GetFriendsList(const LWOOBJID charId) {
	auto [_, friendsList] = ExecuteSelect(
		R"QUERY(
			SELECT fr.requested_player AS player, best_friend AS bff, ci.name AS name FROM 
			(
				SELECT CASE 
				WHEN player_id = ? THEN friend_id 
				WHEN friend_id = ? THEN player_id 
				END AS requested_player, best_friend FROM friends
			) AS fr 
			JOIN charinfo AS ci ON ci.id = fr.requested_player 
			WHERE fr.requested_player IS NOT NULL AND fr.requested_player != ?;
		)QUERY", charId, charId, charId);

	std::vector<FriendData> toReturn;

	while (!friendsList.eof()) {
		FriendData fd;
		fd.friendID = friendsList.getInt64Field("player");
		fd.isBestFriend = friendsList.getIntField("bff") == 3; // 0 = friends, 1 = left_requested, 2 = right_requested, 3 = both_accepted - are now bffs
		fd.friendName = friendsList.getStringField("name");

		toReturn.push_back(fd);
		friendsList.nextRow();
	}

	return toReturn;
}

std::optional<IFriends::BestFriendStatus> SQLiteDatabase::GetBestFriendStatus(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) {
	auto [_, result] = ExecuteSelect("SELECT * FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;",
		playerCharacterId,
		friendCharacterId,
		friendCharacterId,
		playerCharacterId
	);

	if (result.eof()) {
		return std::nullopt;
	}

	IFriends::BestFriendStatus toReturn;
	toReturn.playerCharacterId = result.getInt64Field("player_id");
	toReturn.friendCharacterId = result.getInt64Field("friend_id");
	toReturn.bestFriendStatus = result.getIntField("best_friend");

	return toReturn;
}

void SQLiteDatabase::SetBestFriendStatus(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId, const uint32_t bestFriendStatus) {
	ExecuteUpdate("UPDATE friends SET best_friend = ? WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?);",
		bestFriendStatus,
		playerCharacterId,
		friendCharacterId,
		friendCharacterId,
		playerCharacterId
	);
}

void SQLiteDatabase::AddFriend(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) {
	ExecuteInsert("INSERT OR IGNORE INTO friends (player_id, friend_id, best_friend) VALUES (?, ?, 0);", playerCharacterId, friendCharacterId);
}

void SQLiteDatabase::RemoveFriend(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) {
	ExecuteDelete("DELETE FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?);",
		playerCharacterId,
		friendCharacterId,
		friendCharacterId,
		playerCharacterId
	);
}
