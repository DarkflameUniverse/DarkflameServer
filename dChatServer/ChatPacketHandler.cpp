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

void ChatPacketHandler::HandleFriendlistRequest(Packet* packet) {
	//Get from the packet which player we want to do something with:
	CINSTREAM;
	LWOOBJID playerID = 0;
	inStream.Read(playerID);
	inStream.Read(playerID);

	auto player = PlayerContainer::Instance().GetPlayerData(playerID);
	if (!player) return;

	//Get our friends list from the Db:
	auto stmt = Database::CreatePreppedStmt("SELECT * FROM friends WHERE player_id = ? OR friend_id = ?");
	stmt->setUInt64(1, playerID);
	stmt->setUInt64(2, playerID);

	std::vector<FriendData> friends;

	auto res = stmt->executeQuery();
	while (res->next()) {
		FriendData fd;
		fd.isFTP = false; // not a thing in DLU
		fd.friendID = res->getInt64(1);
		if (fd.friendID == playerID) fd.friendID = res->getUInt64(2);

		fd.isBestFriend = res->getInt(3) == 2; //0 = friends, 1 = requested, 2 = bffs

		//We need to find their name as well:
		{
			auto stmt = Database::CreatePreppedStmt("SELECT name FROM charinfo WHERE id=? limit 1");
			stmt->setInt(1, fd.friendID);

			auto res = stmt->executeQuery();
			while (res->next()) {
				fd.friendName = res->getString(1);
			}

			delete res;
			delete stmt;
		}

		//Now check if they're online:
		auto fr = PlayerContainer::Instance().GetPlayerData(fd.friendID);
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
	delete stmt;

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

	//PacketUtils::SavePacket("FriendRequest.bin", (char*)inStream.GetData(), inStream.GetNumberOfBytesUsed());

	//We need to check to see if the player is actually online or not:
	auto targetData = PlayerContainer::Instance().GetPlayerData(playerName);
	if (targetData) {
		SendFriendRequest(targetData, PlayerContainer::Instance().GetPlayerData(playerID));
	}
}

void ChatPacketHandler::HandleFriendResponse(Packet* packet) {
	CINSTREAM;
	LWOOBJID playerID;
	inStream.Read(playerID);
	inStream.Read(playerID);
	
	uint8_t responseCode = packet->data[0x14];
	std::string friendName = PacketUtils::ReadString(0x15, packet, true);

	Game::logger->Log("ChatPacketHandler", "Friend response code: %i\n", responseCode);

	if (responseCode != 0) return; //If we're not accepting the request, end here, do not insert to friends table.

	PacketUtils::SavePacket("HandleFriendResponse.bin", (char*)inStream.GetData(), inStream.GetNumberOfBytesUsed());

	//Now to try and find both of these:
	auto goonA = PlayerContainer::Instance().GetPlayerData(playerID);
	auto goonB = PlayerContainer::Instance().GetPlayerData(friendName);
	if (!goonA || !goonB) return;

	SendFriendResponse(goonB, goonA, responseCode);
	SendFriendResponse(goonA, goonB, responseCode); //Do we need to send it to both? I think so so both get the updated friendlist but... idk.

	auto stmt = Database::CreatePreppedStmt("INSERT INTO `friends`(`player_id`, `friend_id`, `best_friend`) VALUES (?,?,?)");
	stmt->setUInt64(1, goonA->playerID);
	stmt->setUInt64(2, goonB->playerID);
	stmt->setInt(3, 0);
	stmt->execute();
	delete stmt;
}

void ChatPacketHandler::HandleRemoveFriend(Packet* packet) {
	CINSTREAM;
	LWOOBJID playerID;
	inStream.Read(playerID);
	inStream.Read(playerID);
	std::string friendName = PacketUtils::ReadString(16, packet, true);

	//we'll have to query the db here to find the user, since you can delete them while they're offline.
	//First, we need to find their ID:
	auto stmt = Database::CreatePreppedStmt("select id from charinfo where name=? limit 1;");
	stmt->setString(1, friendName.c_str());

	LWOOBJID friendID = 0;
	auto res = stmt->executeQuery();
	while (res->next()) {
		friendID = res->getUInt64(1);
	}

	delete res;
	delete stmt;

	//Set our bits to convert to the BIG BOY objectID.
	friendID = GeneralUtils::ClearBit(friendID, OBJECT_BIT_CHARACTER);
	friendID = GeneralUtils::ClearBit(friendID, OBJECT_BIT_PERSISTENT);

	//YEET:
	auto deletestmt = Database::CreatePreppedStmt("DELETE FROM `friends` WHERE player_id=? AND friend_id=? LIMIT 1");
	deletestmt->setUInt64(1, playerID);
	deletestmt->setUInt64(2, friendID);
	deletestmt->execute();
	delete deletestmt;

	//because I'm lazy and they can be reversed:
	{
		auto deletestmt = Database::CreatePreppedStmt("DELETE FROM `friends` WHERE player_id=? AND friend_id=? LIMIT 1");
		deletestmt->setUInt64(1, friendID);
		deletestmt->setUInt64(2, playerID);
		deletestmt->execute();
		delete deletestmt;
	}

	//Now, we need to send an update to notify the sender (and possibly, receiver) that their friendship has been ended:
	auto goonA = PlayerContainer::Instance().GetPlayerData(playerID);
	if (goonA) {
		SendRemoveFriend(goonA, friendName, true);
	}
	
	auto goonB = PlayerContainer::Instance().GetPlayerData(friendID);
	if (!goonB) return;
	std::string goonAName = GeneralUtils::UTF16ToWTF8(PlayerContainer::Instance().GetName(playerID));
	SendRemoveFriend(goonB, goonAName, true);
}

void ChatPacketHandler::HandleChatMessage(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);

	auto* sender = PlayerContainer::Instance().GetPlayerData(playerID);

	if (sender == nullptr) return;

	if (PlayerContainer::Instance().GetIsMuted(sender)) return;

	const auto senderName = std::string(sender->playerName.C_String());

	inStream.SetReadOffset(0x14 * 8);

	uint8_t channel = 0;
	inStream.Read(channel);
	
	std::string message = PacketUtils::ReadString(0x66, packet, true);

	Game::logger->Log("ChatPacketHandler", "Got a message from (%s) [%d]: %s\n", senderName.c_str(), channel, message.c_str());

	//PacketUtils::SavePacket("chat.bin", reinterpret_cast<char*>(packet->data), packet->length);

	if (channel != 8) return;

	auto* team = PlayerContainer::Instance().GetTeam(playerID);

	if (team == nullptr) return;

	for (const auto memberId : team->memberIDs)
	{
		auto* otherMember = PlayerContainer::Instance().GetPlayerData(memberId);

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
	auto goonA = PlayerContainer::Instance().GetPlayerData(senderID);
	auto goonB = PlayerContainer::Instance().GetPlayerData(receiverName);
	if (!goonA || !goonB) return;

	if (PlayerContainer::Instance().GetIsMuted(goonA)) return;

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

	auto* player = PlayerContainer::Instance().GetPlayerData(playerID);

	if (player == nullptr)
	{
		return;
	}

	auto* team = PlayerContainer::Instance().GetTeam(playerID);

	if (team == nullptr)
	{
		team = PlayerContainer::Instance().CreateTeam(playerID);
	}

	auto* other = PlayerContainer::Instance().GetPlayerData(invitedPlayer);

	if (other == nullptr)
	{
		return;
	}

	if (PlayerContainer::Instance().GetTeam(other->playerID) != nullptr)
	{
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

	auto* team = PlayerContainer::Instance().GetTeam(leaderID);

	if (team == nullptr)
	{
		Game::logger->Log("ChatPacketHandler", "Failed to find team for leader (%llu)\n", leaderID);

		team = PlayerContainer::Instance().GetTeam(playerID);
	}
	
	if (team == nullptr)
	{
		Game::logger->Log("ChatPacketHandler", "Failed to find team for player (%llu)\n", playerID);
		return;
	}

	PlayerContainer::Instance().AddMember(team, playerID);
}

void ChatPacketHandler::HandleTeamLeave(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);
	uint32_t size = 0;
	inStream.Read(size);

	auto* team = PlayerContainer::Instance().GetTeam(playerID);

	Game::logger->Log("ChatPacketHandler", "(%llu) leaving team\n", playerID);

	if (team != nullptr)
	{
		PlayerContainer::Instance().RemoveMember(team, playerID, false, false, true);
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

	auto* kicked = PlayerContainer::Instance().GetPlayerData(kickedPlayer);

	LWOOBJID kickedId = LWOOBJID_EMPTY;

	if (kicked != nullptr)
	{
		kickedId = kicked->playerID;
	}
	else
	{
		kickedId = PlayerContainer::Instance().GetId(GeneralUtils::ASCIIToUTF16(kickedPlayer));
	}

	if (kickedId == LWOOBJID_EMPTY) return;

	auto* team = PlayerContainer::Instance().GetTeam(playerID);

	if (team != nullptr)
	{
		if (team->leaderID != playerID || team->leaderID == kickedId) return;

		PlayerContainer::Instance().RemoveMember(team, kickedId, false, true, false);
	}

	//PacketUtils::SavePacket("kick.bin", reinterpret_cast<char*>(packet->data), packet->length);
}

void ChatPacketHandler::HandleTeamPromote(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);
	
	std::string promotedPlayer = PacketUtils::ReadString(0x14, packet, true);

	Game::logger->Log("ChatPacketHandler", "(%llu) promoting (%s) to team leader\n", playerID, promotedPlayer.c_str());

	auto* promoted = PlayerContainer::Instance().GetPlayerData(promotedPlayer);

	if (promoted == nullptr) return;

	auto* team = PlayerContainer::Instance().GetTeam(playerID);

	if (team != nullptr)
	{
		if (team->leaderID != playerID) return;

		PlayerContainer::Instance().PromoteMember(team, promoted->playerID);
	}

	//PacketUtils::SavePacket("promote.bin", reinterpret_cast<char*>(packet->data), packet->length);
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

	auto* team = PlayerContainer::Instance().GetTeam(playerID);

	if (team != nullptr)
	{
		if (team->leaderID != playerID) return;

		team->lootFlag = option;

		PlayerContainer::Instance().TeamStatusUpdate(team);
	
		PlayerContainer::Instance().UpdateTeamsOnWorld(team, false);
	}

	//PacketUtils::SavePacket("option.bin", reinterpret_cast<char*>(packet->data), packet->length);
}

void ChatPacketHandler::HandleTeamStatusRequest(Packet* packet) 
{
	CINSTREAM;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	inStream.Read(playerID);

	auto* team = PlayerContainer::Instance().GetTeam(playerID);
	auto* data = PlayerContainer::Instance().GetPlayerData(playerID);

	if (team != nullptr && data != nullptr)
	{
		if (team->local && data->zoneID.GetMapID() != team->zoneId.GetMapID() && data->zoneID.GetCloneID() != team->zoneId.GetCloneID())
		{
			PlayerContainer::Instance().RemoveMember(team, playerID, false, false, true, true);

			return;
		}

		if (team->memberIDs.size() <= 1 && !team->local)
		{
			PlayerContainer::Instance().DisbandTeam(team);

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

		PlayerContainer::Instance().TeamStatusUpdate(team);

		const auto leaderName = GeneralUtils::ASCIIToUTF16(std::string(data->playerName.C_String()));

		for (const auto memberId : team->memberIDs)
		{
			auto* otherMember = PlayerContainer::Instance().GetPlayerData(memberId);

			if (memberId == playerID) continue;

			const auto memberName = PlayerContainer::Instance().GetName(memberId);
			
			//ChatPacketHandler::SendTeamAddPlayer(otherMember, false, false, false, data->playerID, leaderName, data->zoneID);
			if (otherMember != nullptr)
			{
				ChatPacketHandler::SendTeamSetOffWorldFlag(otherMember, data->playerID, data->zoneID);
			}
			ChatPacketHandler::SendTeamAddPlayer(data, false, team->local, false, memberId, memberName, otherMember != nullptr ? otherMember->zoneID : LWOZONEID(0, 0, 0));
		}

		PlayerContainer::Instance().UpdateTeamsOnWorld(team, false);
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
