#include "MySQLDatabase.h"

std::vector<FriendData> MySQLDatabase::GetFriendsList(const LWOOBJID charId) {
	auto friendsList = ExecuteSelect(
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
	toReturn.reserve(friendsList->rowsCount());

	while (friendsList->next()) {
		FriendData fd;
		fd.friendID = friendsList->getUInt64("player");
		fd.isBestFriend = friendsList->getInt("bff") == 3; // 0 = friends, 1 = left_requested, 2 = right_requested, 3 = both_accepted - are now bffs
		fd.friendName = friendsList->getString("name").c_str();

		toReturn.push_back(fd);
	}

	return toReturn;
}

std::optional<IFriends::BestFriendStatus> MySQLDatabase::GetBestFriendStatus(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) {
	auto result = ExecuteSelect("SELECT * FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;",
		playerCharacterId,
		friendCharacterId,
		friendCharacterId,
		playerCharacterId
	);

	if (!result->next()) {
		return std::nullopt;
	}

	IFriends::BestFriendStatus toReturn;
	toReturn.playerCharacterId = result->getUInt64("player_id");
	toReturn.friendCharacterId = result->getUInt64("friend_id");
	toReturn.bestFriendStatus = result->getUInt("best_friend");

	return toReturn;
}

void MySQLDatabase::SetBestFriendStatus(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId, const uint32_t bestFriendStatus) {
	ExecuteUpdate("UPDATE friends SET best_friend = ? WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;",
		bestFriendStatus,
		playerCharacterId,
		friendCharacterId,
		friendCharacterId,
		playerCharacterId
	);
}

void MySQLDatabase::AddFriend(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) {
	ExecuteInsert("INSERT IGNORE INTO friends (player_id, friend_id, best_friend) VALUES (?, ?, 0);", playerCharacterId, friendCharacterId);
}

void MySQLDatabase::RemoveFriend(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) {
	ExecuteDelete("DELETE FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;",
		playerCharacterId,
		friendCharacterId,
		friendCharacterId,
		playerCharacterId
	);
}
