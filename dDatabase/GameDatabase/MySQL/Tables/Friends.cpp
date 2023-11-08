#include "MySQLDatabase.h"

std::vector<FriendData> MySQLDatabase::GetFriendsList(const uint32_t charId) {
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
		fd.friendID = friendsList->getUInt("player");
		fd.isBestFriend = friendsList->getInt("bff") == 3; // 0 = friends, 1 = left_requested, 2 = right_requested, 3 = both_accepted - are now bffs
		fd.friendName = friendsList->getString("name").c_str();

		toReturn.push_back(fd);
	}

	return toReturn;
}

std::optional<IFriends::BestFriendStatus> MySQLDatabase::GetBestFriendStatus(const uint32_t playerCharacterId, const uint32_t friendCharacterId) {
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
	toReturn.playerAccountId = result->getUInt("player_id");
	toReturn.friendAccountId = result->getUInt("friend_id");
	toReturn.bestFriendStatus = result->getUInt("best_friend");

	return toReturn;
}

void MySQLDatabase::SetBestFriendStatus(const uint32_t playerCharacterId, const uint32_t friendCharacterId, const uint32_t bestFriendStatus) {
	ExecuteUpdate("UPDATE friends SET best_friend = ? WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;",
		bestFriendStatus,
		playerCharacterId,
		friendCharacterId,
		friendCharacterId,
		playerCharacterId
	);
}

void MySQLDatabase::AddFriend(const uint32_t playerCharacterId, const uint32_t friendCharacterId) {
	ExecuteInsert("INSERT IGNORE INTO friends (player_id, friend_id, best_friend) VALUES (?, ?, 0);", playerCharacterId, friendCharacterId);
}

void MySQLDatabase::RemoveFriend(const uint32_t playerCharacterId, const uint32_t friendCharacterId) {
	ExecuteDelete("DELETE FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;",
		playerCharacterId,
		friendCharacterId,
		friendCharacterId,
		playerCharacterId
	);
}
