#include "ChatPacketHandler.h"
#include "PlayerContainer.h"
#include "Database.h"
#include <vector>
#include "PacketUtils.h"
#include "dMessageIdentifiers.h"
#include "Game.h"
#include "dServer.h"
#include "GeneralUtils.h"
#include "dLogger.h"

extern PlayerContainer playerContainer;

void ChatPacketHandler::HandleFriendlistRequest(Packet* packet) {
	//Get from the packet which player we want to do something with:
	CINSTREAM;
	LWOOBJID playerID = 0;
	inStream.Read(playerID);
	inStream.Read(playerID);

	auto player = playerContainer.GetPlayerData(playerID);
	if (!player) return;

	//Get our friends list from the Db.  Using a derived table since the friend of a player can be in either column.
	auto stmt = Database::CreatePreppedStmt(
		"SELECT fr.requested_player, best_friend, ci.name FROM "
		"(SELECT CASE "
			"WHEN player_id = ? THEN friend_id "
			"WHEN friend_id = ? THEN player_id "
		"END AS requested_player, best_friend FROM friends) AS fr "
		"JOIN charinfo AS ci ON ci.id = fr.requested_player "
		"WHERE fr.requested_player IS NOT NULL;");
	stmt->setUInt(1, static_cast<uint32_t>(playerID));
	stmt->setUInt(2, static_cast<uint32_t>(playerID));

	std::vector<FriendData> friends;

	auto res = stmt->executeQuery();
	while (res->next()) {
		FriendData fd;
		fd.isFTP = false; // not a thing in DLU
		fd.friendID = res->getUInt(1);
		GeneralUtils::SetBit(fd.friendID, static_cast<size_t>(eObjectBits::OBJECT_BIT_PERSISTENT));
		GeneralUtils::SetBit(fd.friendID, static_cast<size_t>(eObjectBits::OBJECT_BIT_CHARACTER));

		fd.isBestFriend = res->getInt(2) == 2; //0 = friends, 1 = requested, 2 = bffs
		fd.friendName = res->getString(3);

		//Now check if they're online:
		auto fr = playerContainer.GetPlayerData(fd.friendID);
		Game::logger->Log("ChatPacketHandler", "friend is %llu\n", fd.friendID);
		if (fr) {
			fd.isOnline = true;
			fd.zoneID = fr->zoneID;

			//Since this friend is online, we need to update them on the fact that we've just logged in:
			SendFriendUpdate(fr, player, 1);
		}
		else {
			fd.isOnline = false;
			fd.zoneID = LWOZONEID();
		}

		friends.push_back(fd);
	}

	delete res;
	res = nullptr;
	delete stmt;
	stmt = nullptr;

	//Now, we need to send the friendlist to the server they came from:
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(playerID);

	//portion that will get routed:
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_GET_FRIENDS_LIST_RESPONSE);
	bitStream.Write<uint8_t>(0);
	bitStream.Write<uint16_t>(1); //Length of packet -- just writing one as it doesn't matter, client skips it.
	bitStream.Write((uint16_t)friends.size());
	
	for (auto& data : friends) {
		data.Serialize(bitStream);
	}

	player->friends = friends;

	SystemAddress sysAddr = player->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::HandleFriendRequest(Packet* packet) {
	CINSTREAM;
	LWOOBJID playerID;
	inStream.Read(playerID);
	inStream.Read(playerID);
	std::string playerName = PacketUtils::ReadString(0x14, packet, true);
	//There's another bool here to determine if it's a best friend request, but we're not handling it right now.

	//We need to check to see if the player is actually online or not:
	auto targetData = playerContainer.GetPlayerData(playerName);
	if (targetData) {
		SendFriendRequest(targetData, playerContainer.GetPlayerData(playerID));
	}
}

void ChatPacketHandler::HandleFriendResponse(Packet* packet) {
	CINSTREAM;
	LWOOBJID playerID;
	inStream.Read(playerID);
	inStream.Read(playerID);
	
	uint8_t responseCode = packet->data[0x14];
	std::string friendName = PacketUtils::ReadString(0x15, packet, true);

	if (responseCode != 0) return; //If we're not accepting the request, end here, do not insert to friends table.

	//Now to try and find both of these:
	auto goonA = playerContainer.GetPlayerData(playerID);
	auto goonB = playerContainer.GetPlayerData(friendName);
	if (!goonA || !goonB) return;

	SendFriendResponse(goonB, goonA, responseCode);
	SendFriendResponse(goonA, goonB, responseCode); //Do we need to send it to both? I think so so both get the updated friendlist but... idk.

	auto stmt = Database::CreatePreppedStmt("INSERT INTO `friends` (`player_id`, `friend_id`, `best_friend`) VALUES (?,?,?)");
	stmt->setUInt(1, static_cast<uint32_t>(goonA->playerID));
	stmt->setUInt(2, static_cast<uint32_t>(goonB->playerID));
	stmt->setInt(3, 0);
	stmt->execute();
	delete stmt;
}

void ChatPacketHandler::HandleRemoveFriend(Packet* packet) {
	CINSTREAM;
	LWOOBJID playerID;
	inStream.Read(playerID);
	inStream.Read(playerID);
	std::string friendName = PacketUtils::ReadString(0x14, packet, true);

	//we'll have to query the db here to find the user, since you can delete them while they're offline.
	//First, we need to find their ID:
	auto stmt = Database::CreatePreppedStmt("SELECT id FROM charinfo WHERE name=? LIMIT 1;");
	stmt->setString(1, friendName.c_str());

	LWOOBJID friendID = 0;
	auto res = stmt->executeQuery();
	while (res->next()) {
		friendID = res->getUInt(1);
	}

	// Convert friendID to LWOOBJID
	GeneralUtils::SetBit(friendID, static_cast<size_t>(eObjectBits::OBJECT_BIT_PERSISTENT));
	GeneralUtils::SetBit(friendID, static_cast<size_t>(eObjectBits::OBJECT_BIT_CHARACTER));

	delete res;
	res = nullptr;
	delete stmt;
	stmt = nullptr;

	auto deletestmt = Database::CreatePreppedStmt("DELETE FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;");
	deletestmt->setUInt(1, static_cast<uint32_t>(playerID));
	deletestmt->setUInt(2, static_cast<uint32_t>(friendID));
	deletestmt->setUInt(3, static_cast<uint32_t>(friendID));
	deletestmt->setUInt(4, static_cast<uint32_t>(playerID));
	deletestmt->execute();

	delete deletestmt;
	deletestmt = nullptr;

	//Now, we need to send an update to notify the sender (and possibly, receiver) that their friendship has been ended:
	auto goonA = playerContainer.GetPlayerData(playerID);
	if (goonA) {
		SendRemoveFriend(goonA, friendName, true);
	}
	
	auto goonB = playerContainer.GetPlayerData(friendID);
	if (!goonB) return;
	std::string goonAName = GeneralUtils::UTF16ToWTF8(playerContainer.GetName(playerID));
	SendRemoveFriend(goonB, goonAName, true);
}

void ChatPacketHandler::HandleChatMessage(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);

	auto* sender = playerContainer.GetPlayerData(playerID);

	if (sender == nullptr) return;

	if (playerContainer.GetIsMuted(sender)) return;

	const auto senderName = std::string(sender->playerName.C_String());

	inStream.SetReadOffset(0x14 * 8);

	uint8_t channel = 0;
	inStream.Read(channel);
	
	std::string message = PacketUtils::ReadString(0x66, packet, true);

	Game::logger->Log("ChatPacketHandler", "Got a message from (%s) [%d]: %s\n", senderName.c_str(), channel, message.c_str());

	if (channel != 8) return;

	auto* team = playerContainer.GetTeam(playerID);

	if (team == nullptr) return;

	for (const auto memberId : team->memberIDs)
	{
		auto* otherMember = playerContainer.GetPlayerData(memberId);

		if (otherMember == nullptr) return;

		const auto otherName = std::string(otherMember->playerName.C_String());

		CBITSTREAM;
		PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
		bitStream.Write(otherMember->playerID);

		PacketUtils::WriteHeader(bitStream, CHAT, MSG_CHAT_PRIVATE_CHAT_MESSAGE);
		bitStream.Write(otherMember->playerID);
		bitStream.Write<uint8_t>(8);
		bitStream.Write<unsigned int>(69);
		PacketUtils::WritePacketWString(senderName, 33, &bitStream);
		bitStream.Write(sender->playerID);
		bitStream.Write<uint16_t>(0);
		bitStream.Write<uint8_t>(0); //not mythran nametag
		PacketUtils::WritePacketWString(otherName, 33, &bitStream);
		bitStream.Write<uint8_t>(0); //not mythran for receiver
		bitStream.Write<uint8_t>(0); //teams?
		PacketUtils::WritePacketWString(message, 512, &bitStream);

		SystemAddress sysAddr = otherMember->sysAddr;
		SEND_PACKET;
	}
}

void ChatPacketHandler::HandlePrivateChatMessage(Packet* packet) {
	LWOOBJID senderID = PacketUtils::ReadPacketS64(0x08, packet);
	std::string receiverName = PacketUtils::ReadString(0x66, packet, true);
	std::string message = PacketUtils::ReadString(0xAA, packet, true);

	//Get the bois:
	auto goonA = playerContainer.GetPlayerData(senderID);
	auto goonB = playerContainer.GetPlayerData(receiverName);
	if (!goonA || !goonB) return;

	if (playerContainer.GetIsMuted(goonA)) return;

	std::string goonAName = goonA->playerName.C_String();
	std::string goonBName = goonB->playerName.C_String();

	//To the sender:
	{
		CBITSTREAM;
		PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
		bitStream.Write(goonA->playerID);

		PacketUtils::WriteHeader(bitStream, CHAT, MSG_CHAT_PRIVATE_CHAT_MESSAGE);
		bitStream.Write(goonA->playerID);
		bitStream.Write<uint8_t>(7);
		bitStream.Write<unsigned int>(69);
		PacketUtils::WritePacketWString(goonAName, 33, &bitStream);
		bitStream.Write(goonA->playerID);
		bitStream.Write<uint16_t>(0);
		bitStream.Write<uint8_t>(0); //not mythran nametag
		PacketUtils::WritePacketWString(goonBName, 33, &bitStream);
		bitStream.Write<uint8_t>(0); //not mythran for receiver
		bitStream.Write<uint8_t>(0); //success
		PacketUtils::WritePacketWString(message, 512, &bitStream);

		SystemAddress sysAddr = goonA->sysAddr;
		SEND_PACKET;
	}

	//To the receiver:
	{
		CBITSTREAM;
		PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
		bitStream.Write(goonB->playerID);

		PacketUtils::WriteHeader(bitStream, CHAT, MSG_CHAT_PRIVATE_CHAT_MESSAGE);
		bitStream.Write(goonA->playerID);
		bitStream.Write<uint8_t>(7);
		bitStream.Write<unsigned int>(69);
		PacketUtils::WritePacketWString(goonAName, 33, &bitStream);
		bitStream.Write(goonA->playerID);
		bitStream.Write<uint16_t>(0);
		bitStream.Write<uint8_t>(0); //not mythran nametag
		PacketUtils::WritePacketWString(goonBName, 33, &bitStream);
		bitStream.Write<uint8_t>(0); //not mythran for receiver
		bitStream.Write<uint8_t>(3); //new whisper
		PacketUtils::WritePacketWString(message, 512, &bitStream);

		SystemAddress sysAddr = goonB->sysAddr;
		SEND_PACKET;
	}
}

void ChatPacketHandler::HandleTeamInvite(Packet* packet)
{
	CINSTREAM;
	LWOOBJID playerID;
	inStream.Read(playerID);
	inStream.Read(playerID);
	std::string invitedPlayer = PacketUtils::ReadString(0x14, packet, true);

	auto* player = playerContainer.GetPlayerData(playerID);

	if (player == nullptr)
	{
		return;
	}

	auto* team = playerContainer.GetTeam(playerID);

	if (team == nullptr)
	{
		team = playerContainer.CreateTeam(playerID);
	}

	auto* other = playerContainer.GetPlayerData(invitedPlayer);

	if (other == nullptr)
	{
		return;
	}

	if (playerContainer.GetTeam(other->playerID) != nullptr)
	{
		return;
	}

	if (team->memberIDs.size() > 3) {
		// no more teams greater than 4

		Game::logger->Log("ChatPacketHandler", "Someone tried to invite a 5th player to a team\n");
		return;
	}

	SendTeamInvite(other, player);

	Game::logger->Log("ChatPacketHandler", "Got team invite: %llu -> %s\n", playerID, invitedPlayer.c_str());
}

void ChatPacketHandler::HandleTeamInviteResponse(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);
	uint32_t size = 0;
	inStream.Read(size);
	char declined = 0;
	inStream.Read(declined);
	LWOOBJID leaderID = LWOOBJID_EMPTY;
	inStream.Read(leaderID);

	Game::logger->Log("ChatPacketHandler", "Accepted invite: %llu -> %llu (%d)\n", playerID, leaderID, declined);

	if (declined)
	{
		return;
	}

	auto* team = playerContainer.GetTeam(leaderID);

	if (team == nullptr)
	{
		Game::logger->Log("ChatPacketHandler", "Failed to find team for leader (%llu)\n", leaderID);

		team = playerContainer.GetTeam(playerID);
	}
	
	if (team == nullptr)
	{
		Game::logger->Log("ChatPacketHandler", "Failed to find team for player (%llu)\n", playerID);
		return;
	}

	playerContainer.AddMember(team, playerID);
}

void ChatPacketHandler::HandleTeamLeave(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);
	uint32_t size = 0;
	inStream.Read(size);

	auto* team = playerContainer.GetTeam(playerID);

	Game::logger->Log("ChatPacketHandler", "(%llu) leaving team\n", playerID);

	if (team != nullptr)
	{
		playerContainer.RemoveMember(team, playerID, false, false, true);
	}
}

void ChatPacketHandler::HandleTeamKick(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);
	
	std::string kickedPlayer = PacketUtils::ReadString(0x14, packet, true);

	Game::logger->Log("ChatPacketHandler", "(%llu) kicking (%s) from team\n", playerID, kickedPlayer.c_str());

	auto* kicked = playerContainer.GetPlayerData(kickedPlayer);

	LWOOBJID kickedId = LWOOBJID_EMPTY;

	if (kicked != nullptr)
	{
		kickedId = kicked->playerID;
	}
	else
	{
		kickedId = playerContainer.GetId(GeneralUtils::ASCIIToUTF16(kickedPlayer));
	}

	if (kickedId == LWOOBJID_EMPTY) return;

	auto* team = playerContainer.GetTeam(playerID);

	if (team != nullptr)
	{
		if (team->leaderID != playerID || team->leaderID == kickedId) return;

		playerContainer.RemoveMember(team, kickedId, false, true, false);
	}
}

void ChatPacketHandler::HandleTeamPromote(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);
	
	std::string promotedPlayer = PacketUtils::ReadString(0x14, packet, true);

	Game::logger->Log("ChatPacketHandler", "(%llu) promoting (%s) to team leader\n", playerID, promotedPlayer.c_str());

	auto* promoted = playerContainer.GetPlayerData(promotedPlayer);

	if (promoted == nullptr) return;

	auto* team = playerContainer.GetTeam(playerID);

	if (team != nullptr)
	{
		if (team->leaderID != playerID) return;

		playerContainer.PromoteMember(team, promoted->playerID);
	}
}

void ChatPacketHandler::HandleTeamLootOption(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);
	uint32_t size = 0;
	inStream.Read(size);
	
	char option;
	inStream.Read(option);

	auto* team = playerContainer.GetTeam(playerID);

	if (team != nullptr)
	{
		if (team->leaderID != playerID) return;

		team->lootFlag = option;

		playerContainer.TeamStatusUpdate(team);
	
		playerContainer.UpdateTeamsOnWorld(team, false);
	}
}

void ChatPacketHandler::HandleTeamStatusRequest(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);

	auto* team = playerContainer.GetTeam(playerID);
	auto* data = playerContainer.GetPlayerData(playerID);

	if (team != nullptr && data != nullptr)
	{
		if (team->local && data->zoneID.GetMapID() != team->zoneId.GetMapID() && data->zoneID.GetCloneID() != team->zoneId.GetCloneID())
		{
			playerContainer.RemoveMember(team, playerID, false, false, true, true);

			return;
		}

		if (team->memberIDs.size() <= 1 && !team->local)
		{
			playerContainer.DisbandTeam(team);

			return;
		}

		if (!team->local)
		{
			ChatPacketHandler::SendTeamSetLeader(data, team->leaderID);
		}
		else
		{
			ChatPacketHandler::SendTeamSetLeader(data, LWOOBJID_EMPTY);
		}

		playerContainer.TeamStatusUpdate(team);

		const auto leaderName = GeneralUtils::ASCIIToUTF16(std::string(data->playerName.C_String()));

		for (const auto memberId : team->memberIDs)
		{
			auto* otherMember = playerContainer.GetPlayerData(memberId);

			if (memberId == playerID) continue;

			const auto memberName = playerContainer.GetName(memberId);
			
			if (otherMember != nullptr)
			{
				ChatPacketHandler::SendTeamSetOffWorldFlag(otherMember, data->playerID, data->zoneID);
			}
			ChatPacketHandler::SendTeamAddPlayer(data, false, team->local, false, memberId, memberName, otherMember != nullptr ? otherMember->zoneID : LWOZONEID(0, 0, 0));
		}

		playerContainer.UpdateTeamsOnWorld(team, false);
	}
}

void ChatPacketHandler::SendTeamInvite(PlayerData* receiver, PlayerData* sender) 
{
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_TEAM_INVITE);

	PacketUtils::WritePacketWString(sender->playerName.C_String(), 33, &bitStream);
	bitStream.Write(sender->playerID);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamInviteConfirm(PlayerData* receiver, bool bLeaderIsFreeTrial, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, uint8_t ucResponseCode, std::u16string wsLeaderName) 
{
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER

	bitStream.Write(receiver->playerID);
	bitStream.Write(GAME_MSG::GAME_MSG_TEAM_INVITE_CONFIRM);

	bitStream.Write(bLeaderIsFreeTrial);
	bitStream.Write(i64LeaderID);
	bitStream.Write(i64LeaderZoneID);
	bitStream.Write<uint32_t>(0); // BinaryBuffe, no clue what's in here
	bitStream.Write(ucLootFlag);
	bitStream.Write(ucNumOfOtherPlayers);
	bitStream.Write(ucResponseCode);
	bitStream.Write(static_cast<uint32_t>(wsLeaderName.size()));
	for (const auto character : wsLeaderName)
	{
		bitStream.Write(character);
	}

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamStatus(PlayerData* receiver, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, std::u16string wsLeaderName)
{
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER

	bitStream.Write(receiver->playerID);
	bitStream.Write(GAME_MSG::GAME_MSG_TEAM_GET_STATUS_RESPONSE);

	bitStream.Write(i64LeaderID);
	bitStream.Write(i64LeaderZoneID);
	bitStream.Write<uint32_t>(0); // BinaryBuffe, no clue what's in here
	bitStream.Write(ucLootFlag);
	bitStream.Write(ucNumOfOtherPlayers);
	bitStream.Write(static_cast<uint32_t>(wsLeaderName.size()));
	for (const auto character : wsLeaderName)
	{
		bitStream.Write(character);
	}

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamSetLeader(PlayerData* receiver, LWOOBJID i64PlayerID) 
{
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER

	bitStream.Write(receiver->playerID);
	bitStream.Write(GAME_MSG::GAME_MSG_TEAM_SET_LEADER);

	bitStream.Write(i64PlayerID);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamAddPlayer(PlayerData* receiver, bool bIsFreeTrial, bool bLocal, bool bNoLootOnDeath, LWOOBJID i64PlayerID, std::u16string wsPlayerName, LWOZONEID zoneID) 
{
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER

	bitStream.Write(receiver->playerID);
	bitStream.Write(GAME_MSG::GAME_MSG_TEAM_ADD_PLAYER);

	bitStream.Write(bIsFreeTrial);
	bitStream.Write(bLocal);
	bitStream.Write(bNoLootOnDeath);
	bitStream.Write(i64PlayerID);
	bitStream.Write(static_cast<uint32_t>(wsPlayerName.size()));
	for (const auto character : wsPlayerName)
	{
		bitStream.Write(character);
	}
	bitStream.Write1();
	if (receiver->zoneID.GetCloneID() == zoneID.GetCloneID())
	{
		zoneID = LWOZONEID(zoneID.GetMapID(), zoneID.GetInstanceID(), 0);
	}
	bitStream.Write(zoneID);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamRemovePlayer(PlayerData* receiver, bool bDisband, bool bIsKicked, bool bIsLeaving, bool bLocal, LWOOBJID i64LeaderID, LWOOBJID i64PlayerID, std::u16string wsPlayerName) 
{
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER

	bitStream.Write(receiver->playerID);
	bitStream.Write(GAME_MSG::GAME_MSG_TEAM_REMOVE_PLAYER);

	bitStream.Write(bDisband);
	bitStream.Write(bIsKicked);
	bitStream.Write(bIsLeaving);
	bitStream.Write(bLocal);
	bitStream.Write(i64LeaderID);
	bitStream.Write(i64PlayerID);
	bitStream.Write(static_cast<uint32_t>(wsPlayerName.size()));
	for (const auto character : wsPlayerName)
	{
		bitStream.Write(character);
	}

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamSetOffWorldFlag(PlayerData* receiver, LWOOBJID i64PlayerID, LWOZONEID zoneID) 
{
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER

	bitStream.Write(receiver->playerID);
	bitStream.Write(GAME_MSG::GAME_MSG_TEAM_SET_OFF_WORLD_FLAG);

	bitStream.Write(i64PlayerID);
	if (receiver->zoneID.GetCloneID() == zoneID.GetCloneID())
	{
		zoneID = LWOZONEID(zoneID.GetMapID(), zoneID.GetInstanceID(), 0);
	}
	bitStream.Write(zoneID);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendFriendUpdate(PlayerData* friendData, PlayerData* playerData, uint8_t notifyType) {
	/*chat notification is displayed if log in / out and friend is updated in friends list
		[u8] - update type
		Update types
		0 - friend logged out
		1 - friend logged in
		2 - friend changed world / updated
		[wstring] - Name of friend
		[u16] - World ID
		[u16] - World Instance
		[u32] - World Clone
		[bool] - is best friend
		[bool] - is FTP*/

	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(friendData->playerID);

	//portion that will get routed:
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_UPDATE_FRIEND_NOTIFY);
	bitStream.Write<uint8_t>(notifyType);

	std::string playerName = playerData->playerName.C_String();

	PacketUtils::WritePacketWString(playerName, 33, &bitStream);

	bitStream.Write(playerData->zoneID.GetMapID());
	bitStream.Write(playerData->zoneID.GetInstanceID());

	if (playerData->zoneID.GetCloneID() == friendData->zoneID.GetCloneID())
	{
		bitStream.Write(0);
	}
	else
	{
		bitStream.Write(playerData->zoneID.GetCloneID());
	}

	bitStream.Write<uint8_t>(0); //isBFF
	bitStream.Write<uint8_t>(0); //isFTP

	SystemAddress sysAddr = friendData->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendFriendRequest(PlayerData* receiver, PlayerData* sender, bool isBFFReq) {
	if (!receiver || !sender) return;

	//Make sure people aren't requesting people that they're already friends with:
	for (auto fr : receiver->friends) {
		if (fr.friendID == sender->playerID) {
			return; //we have this player as a friend, yeet this function so it doesn't send another request.
		}
	}

	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_ADD_FRIEND_REQUEST);
	PacketUtils::WritePacketWString(sender->playerName.C_String(), 33, &bitStream);
	bitStream.Write<uint8_t>(0);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendFriendResponse(PlayerData* receiver, PlayerData* sender, uint8_t responseCode) {
	if (!receiver || !sender) return;

	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_ADD_FRIEND_RESPONSE);
	bitStream.Write<uint8_t>(responseCode);
	bitStream.Write<uint8_t>(1); //isOnline
	PacketUtils::WritePacketWString(sender->playerName.C_String(), 33, &bitStream);
	bitStream.Write(sender->playerID);
	bitStream.Write(sender->zoneID);
	bitStream.Write<uint8_t>(0); //isBFF
	bitStream.Write<uint8_t>(0); //isFTP

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendRemoveFriend(PlayerData* receiver, std::string& personToRemove, bool isSuccessful) {
	if (!receiver) return;

	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT_INTERNAL, MSG_CHAT_INTERNAL_ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_REMOVE_FRIEND_RESPONSE);
	bitStream.Write<uint8_t>(isSuccessful); //isOnline
	PacketUtils::WritePacketWString(personToRemove, 33, &bitStream);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}
