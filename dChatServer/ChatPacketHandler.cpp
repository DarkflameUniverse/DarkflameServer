#include "ChatPacketHandler.h"
#include "PlayerContainer.h"
#include "Database.h"
#include <vector>
#include "PacketUtils.h"
#include "BitStreamUtils.h"
#include "Game.h"
#include "dServer.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "eAddFriendResponseCode.h"
#include "eAddFriendResponseType.h"
#include "RakString.h"
#include "dConfig.h"
#include "eObjectBits.h"
#include "eConnectionType.h"
#include "eChatMessageType.h"
#include "eChatInternalMessageType.h"
#include "eClientMessageType.h"
#include "eGameMessageType.h"
#include "eGuildLeaveReason.h"

extern PlayerContainer playerContainer;

void ChatPacketHandler::HandleFriendlistRequest(Packet* packet) {
	//Get from the packet which player we want to do something with:
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = 0;
	inStream.Read(playerID);

	auto player = playerContainer.GetPlayerData(playerID);
	if (!player) return;

	auto friendsList = Database::Get()->GetFriendsList(playerID);
	for (const auto& friendData : friendsList) {
		FriendData fd;
		fd.isFTP = false; // not a thing in DLU
		fd.friendID = friendData.friendID;
		GeneralUtils::SetBit(fd.friendID, eObjectBits::PERSISTENT);
		GeneralUtils::SetBit(fd.friendID, eObjectBits::CHARACTER);

		fd.isBestFriend = friendData.isBestFriend; //0 = friends, 1 = left_requested, 2 = right_requested, 3 = both_accepted - are now bffs
		if (fd.isBestFriend) player->countOfBestFriends += 1;
		fd.friendName = friendData.friendName;

		//Now check if they're online:
		auto fr = playerContainer.GetPlayerData(fd.friendID);

		if (fr) {
			fd.isOnline = true;
			fd.zoneID = fr->zoneID;

			//Since this friend is online, we need to update them on the fact that we've just logged in:
			SendFriendUpdate(fr, player, 1, fd.isBestFriend);
		} else {
			fd.isOnline = false;
			fd.zoneID = LWOZONEID();
		}

		player->friends.push_back(fd);
	}

	//Now, we need to send the friendlist to the server they came from:
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(playerID);

	//portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::GET_FRIENDS_LIST_RESPONSE);
	bitStream.Write<uint8_t>(0);
	bitStream.Write<uint16_t>(1); //Length of packet -- just writing one as it doesn't matter, client skips it.
	bitStream.Write((uint16_t)player->friends.size());

	for (auto& data : player->friends) {
		data.Serialize(bitStream);
	}

	SystemAddress sysAddr = player->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::HandleFriendRequest(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID requestorPlayerID;
	inStream.Read(requestorPlayerID);
	uint32_t spacing{};
	inStream.Read(spacing);
	std::string playerName = "";
	uint16_t character;
	bool noMoreLettersInName = false;

	for (uint32_t j = 0; j < 33; j++) {
		inStream.Read(character);
		if (character == '\0') noMoreLettersInName = true;
		if (!noMoreLettersInName) playerName.push_back(static_cast<char>(character));
	}

	char isBestFriendRequest{};
	inStream.Read(isBestFriendRequest);

	auto requestor = playerContainer.GetPlayerData(requestorPlayerID);
	if (!requestor) {
		LOG("No requestor player %llu sent to %s found.", requestorPlayerID, playerName.c_str());
		return;
	}

	if (requestor->playerName == playerName) {
		SendFriendResponse(requestor, requestor, eAddFriendResponseType::MYTHRAN);
		return;
	};
	std::unique_ptr<PlayerData> requestee(playerContainer.GetPlayerData(playerName));

	// Check if player is online first
	if (isBestFriendRequest && !requestee) {
		for (auto friendDataCandidate : requestor->friends) {
			if (friendDataCandidate.friendName == playerName) {
				requestee.reset(new PlayerData());
				// Setup the needed info since you can add a best friend offline.
				requestee->playerID = friendDataCandidate.friendID;
				requestee->playerName = friendDataCandidate.friendName;
				requestee->zoneID = LWOZONEID();

				FriendData requesteeFriendData{};
				requesteeFriendData.friendID = requestor->playerID;
				requesteeFriendData.friendName = requestor->playerName;
				requesteeFriendData.isFTP = false;
				requesteeFriendData.isOnline = false;
				requesteeFriendData.zoneID = requestor->zoneID;
				requestee->friends.push_back(requesteeFriendData);
				requestee->sysAddr = UNASSIGNED_SYSTEM_ADDRESS;
				break;
			}
		}
	}

	// If at this point we dont have a target, then they arent online and we cant send the request.
	// Send the response code that corresponds to what the error is.
	if (!requestee) {
		requestee.reset(new PlayerData());
		requestee->playerName = playerName;
		auto responseType = Database::Get()->GetCharacterInfo(playerName)
			? eAddFriendResponseType::NOTONLINE
			: eAddFriendResponseType::INVALIDCHARACTER;

		SendFriendResponse(requestor, requestee.get(), responseType);
		return;
	}

	if (isBestFriendRequest) {

		uint8_t oldBestFriendStatus{};
		uint8_t bestFriendStatus{};
		auto bestFriendInfo = Database::Get()->GetBestFriendStatus(requestorPlayerID, requestee->playerID);
		if (bestFriendInfo) {
			// Get the IDs
			LWOOBJID queryPlayerID = bestFriendInfo->playerCharacterId;
			LWOOBJID queryFriendID = bestFriendInfo->friendCharacterId;
			oldBestFriendStatus = bestFriendInfo->bestFriendStatus;
			bestFriendStatus = oldBestFriendStatus;

			// Set the bits
			GeneralUtils::SetBit(queryPlayerID, eObjectBits::CHARACTER);
			GeneralUtils::SetBit(queryPlayerID, eObjectBits::PERSISTENT);
			GeneralUtils::SetBit(queryFriendID, eObjectBits::CHARACTER);
			GeneralUtils::SetBit(queryFriendID, eObjectBits::PERSISTENT);

			// Since this player can either be the friend of someone else or be friends with someone else
			// their column in the database determines what bit gets set.  When the value hits 3, they
			// are now best friends with the other player.
			if (queryPlayerID == requestorPlayerID) {
				bestFriendStatus |= 1ULL << 0;
			} else {
				bestFriendStatus |= 1ULL << 1;
			}
		}

		// Only do updates if there was a change in the bff status.
		if (oldBestFriendStatus != bestFriendStatus) {
			auto maxBestFriends = playerContainer.GetMaxNumberOfBestFriends();
			if (requestee->countOfBestFriends >= maxBestFriends || requestor->countOfBestFriends >= maxBestFriends) {
				if (requestee->countOfBestFriends >= maxBestFriends) {
					SendFriendResponse(requestor, requestee.get(), eAddFriendResponseType::THEIRFRIENDLISTFULL, false);
				}
				if (requestor->countOfBestFriends >= maxBestFriends) {
					SendFriendResponse(requestor, requestee.get(), eAddFriendResponseType::YOURFRIENDSLISTFULL, false);
				}
			} else {
				// Then update the database with this new info.
				Database::Get()->SetBestFriendStatus(requestorPlayerID, requestee->playerID, bestFriendStatus);
				// Sent the best friend update here if the value is 3
				if (bestFriendStatus == 3U) {
					requestee->countOfBestFriends += 1;
					requestor->countOfBestFriends += 1;
					if (requestee->sysAddr != UNASSIGNED_SYSTEM_ADDRESS) SendFriendResponse(requestee.get(), requestor, eAddFriendResponseType::ACCEPTED, false, true);
					if (requestor->sysAddr != UNASSIGNED_SYSTEM_ADDRESS) SendFriendResponse(requestor, requestee.get(), eAddFriendResponseType::ACCEPTED, false, true);
					for (auto& friendData : requestor->friends) {
						if (friendData.friendID == requestee->playerID) {
							friendData.isBestFriend = true;
						}
					}
					for (auto& friendData : requestee->friends) {
						if (friendData.friendID == requestor->playerID) {
							friendData.isBestFriend = true;
						}
					}
				}
			}
		} else {
			if (requestor->sysAddr != UNASSIGNED_SYSTEM_ADDRESS) SendFriendResponse(requestor, requestee.get(), eAddFriendResponseType::WAITINGAPPROVAL, true, true);
		}
	} else {
		auto maxFriends = playerContainer.GetMaxNumberOfFriends();
		if (requestee->friends.size() >= maxFriends) {
			SendFriendResponse(requestor, requestee.get(), eAddFriendResponseType::THEIRFRIENDLISTFULL, false);
		} else if (requestor->friends.size() >= maxFriends) {
			SendFriendResponse(requestor, requestee.get(), eAddFriendResponseType::YOURFRIENDSLISTFULL, false);
		} else {
			// Do not send this if we are requesting to be a best friend.
			SendFriendRequest(requestee.get(), requestor);
		}
	}

	// If the player is actually a player and not a ghost one defined above, release it from being deleted.
	if (requestee->sysAddr != UNASSIGNED_SYSTEM_ADDRESS) requestee.release();
}

void ChatPacketHandler::HandleFriendResponse(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID;
	inStream.Read(playerID);

	eAddFriendResponseCode clientResponseCode = static_cast<eAddFriendResponseCode>(packet->data[0x14]);
	std::string friendName = PacketUtils::ReadString(0x15, packet, true);

	//Now to try and find both of these:
	auto requestor = playerContainer.GetPlayerData(playerID);
	auto requestee = playerContainer.GetPlayerData(friendName);
	if (!requestor || !requestee) return;

	eAddFriendResponseType serverResponseCode{};
	uint8_t isAlreadyBestFriends = 0U;
	// We need to convert this response code to one we can actually send back to the client.
	switch (clientResponseCode) {
	case eAddFriendResponseCode::ACCEPTED:
		serverResponseCode = eAddFriendResponseType::ACCEPTED;
		break;
	case eAddFriendResponseCode::BUSY:
		serverResponseCode = eAddFriendResponseType::BUSY;
		break;
	case eAddFriendResponseCode::CANCELLED:
		serverResponseCode = eAddFriendResponseType::CANCELLED;
		break;
	case eAddFriendResponseCode::REJECTED:
		serverResponseCode = eAddFriendResponseType::DECLINED;
		break;
	}

	// Now that we have handled the base cases, we need to check the other cases.
	if (serverResponseCode == eAddFriendResponseType::ACCEPTED) {
		for (auto friendData : requestor->friends) {
			if (friendData.friendID == requestee->playerID) {
				serverResponseCode = eAddFriendResponseType::ALREADYFRIEND;
				if (friendData.isBestFriend) {
					isAlreadyBestFriends = 1U;
				}
			}
		}
	}

	// This message is NOT sent for best friends and is handled differently for those requests.
	if (serverResponseCode == eAddFriendResponseType::ACCEPTED) {
		// Add the each player to the others friend list.
		FriendData requestorData;
		requestorData.zoneID = requestor->zoneID;
		requestorData.friendID = requestor->playerID;
		requestorData.friendName = requestor->playerName;
		requestorData.isBestFriend = false;
		requestorData.isFTP = false;
		requestorData.isOnline = true;
		requestee->friends.push_back(requestorData);

		FriendData requesteeData;
		requesteeData.zoneID = requestee->zoneID;
		requesteeData.friendID = requestee->playerID;
		requesteeData.friendName = requestee->playerName;
		requesteeData.isBestFriend = false;
		requesteeData.isFTP = false;
		requesteeData.isOnline = true;
		requestor->friends.push_back(requesteeData);

		Database::Get()->AddFriend(requestor->playerID, requestee->playerID);
	}

	if (serverResponseCode != eAddFriendResponseType::DECLINED) SendFriendResponse(requestor, requestee, serverResponseCode, isAlreadyBestFriends);
	if (serverResponseCode != eAddFriendResponseType::ALREADYFRIEND) SendFriendResponse(requestee, requestor, serverResponseCode, isAlreadyBestFriends);
}

void ChatPacketHandler::HandleRemoveFriend(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID;
	inStream.Read(playerID);
	std::string friendName = PacketUtils::ReadString(0x14, packet, true);

	//we'll have to query the db here to find the user, since you can delete them while they're offline.
	//First, we need to find their ID:
	LWOOBJID friendID = 0;
	auto friendIdResult = Database::Get()->GetCharacterInfo(friendName);
	if (friendIdResult) {
		friendID = friendIdResult->id;
	}

	// Convert friendID to LWOOBJID
	GeneralUtils::SetBit(friendID, eObjectBits::PERSISTENT);
	GeneralUtils::SetBit(friendID, eObjectBits::CHARACTER);

	Database::Get()->RemoveFriend(playerID, friendID);

	//Now, we need to send an update to notify the sender (and possibly, receiver) that their friendship has been ended:
	auto goonA = playerContainer.GetPlayerData(playerID);
	if (goonA) {
		// Remove the friend from our list of friends
		for (auto friendData = goonA->friends.begin(); friendData != goonA->friends.end(); friendData++) {
			if ((*friendData).friendID == friendID) {
				if ((*friendData).isBestFriend) --goonA->countOfBestFriends;
				goonA->friends.erase(friendData);
				break;
			}
		}
		SendRemoveFriend(goonA, friendName, true);
	}

	auto goonB = playerContainer.GetPlayerData(friendID);
	if (!goonB) return;
	// Do it again for other person
	for (auto friendData = goonB->friends.begin(); friendData != goonB->friends.end(); friendData++) {
		if ((*friendData).friendID == playerID) {
			if ((*friendData).isBestFriend) --goonB->countOfBestFriends;
			goonB->friends.erase(friendData);
			break;
		}
	}

	std::string goonAName = GeneralUtils::UTF16ToWTF8(playerContainer.GetName(playerID));
	SendRemoveFriend(goonB, goonAName, true);
}

void ChatPacketHandler::HandleChatMessage(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);

	auto* sender = playerContainer.GetPlayerData(playerID);

	if (sender == nullptr) return;

	if (playerContainer.GetIsMuted(sender)) return;

	const auto senderName = std::string(sender->playerName.c_str());

	inStream.SetReadOffset(0x14 * 8);

	uint8_t channel = 0;
	inStream.Read(channel);

	std::string message = PacketUtils::ReadString(0x66, packet, true, 512);

	LOG("Got a message from (%s) [%d]: %s", senderName.c_str(), channel, message.c_str());

	if (channel == 8) {

		auto* team = playerContainer.GetTeam(playerID);
		if (!team) return;

		for (const auto memberId : team->memberIDs) {
			auto* otherMember = playerContainer.GetPlayerData(memberId);
			if (!otherMember) return;
			const auto otherName = std::string(otherMember->playerName.c_str());

			CBITSTREAM;
			BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
			bitStream.Write(otherMember->playerID);

			BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::PRIVATE_CHAT_MESSAGE);
			bitStream.Write(otherMember->playerID);
			bitStream.Write<uint8_t>(8);
			bitStream.Write<unsigned int>(69);
			bitStream.Write(LUWString(senderName));
			bitStream.Write(sender->playerID);
			bitStream.Write<uint16_t>(0);
			bitStream.Write<uint8_t>(0); //not mythran nametag
			bitStream.Write(LUWString(otherName));
			bitStream.Write<uint8_t>(0); //not mythran for receiver
			bitStream.Write<uint8_t>(0); //teams?
			bitStream.Write(LUWString(message, 512));

			SystemAddress sysAddr = otherMember->sysAddr;
			SEND_PACKET;
		}
	} else LOG("Private chat channel is %i", channel);
}

void ChatPacketHandler::HandlePrivateChatMessage(Packet* packet) {
	LWOOBJID senderID = PacketUtils::ReadS64(0x08, packet);
	std::string receiverName = PacketUtils::ReadString(0x66, packet, true);
	std::string message = PacketUtils::ReadString(0xAA, packet, true, 512);

	//Get the bois:
	auto goonA = playerContainer.GetPlayerData(senderID);
	auto goonB = playerContainer.GetPlayerData(receiverName);
	if (!goonA || !goonB) return;

	if (playerContainer.GetIsMuted(goonA)) return;

	std::string goonAName = goonA->playerName.c_str();
	std::string goonBName = goonB->playerName.c_str();

	//To the sender:
	{
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
		bitStream.Write(goonA->playerID);

		BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::PRIVATE_CHAT_MESSAGE);
		bitStream.Write(goonA->playerID);
		bitStream.Write<uint8_t>(7);
		bitStream.Write<unsigned int>(69);
		bitStream.Write(LUWString(goonAName));
		bitStream.Write(goonA->playerID);
		bitStream.Write<uint16_t>(0);
		bitStream.Write<uint8_t>(0); //not mythran nametag
		bitStream.Write(LUWString(goonBName));
		bitStream.Write<uint8_t>(0); //not mythran for receiver
		bitStream.Write<uint8_t>(0); //success
		bitStream.Write(LUWString(message, 512));

		SystemAddress sysAddr = goonA->sysAddr;
		SEND_PACKET;
	}

	//To the receiver:
	{
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
		bitStream.Write(goonB->playerID);

		BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::PRIVATE_CHAT_MESSAGE);
		bitStream.Write(goonA->playerID);
		bitStream.Write<uint8_t>(7);
		bitStream.Write<unsigned int>(69);
		bitStream.Write(LUWString(goonAName));
		bitStream.Write(goonA->playerID);
		bitStream.Write<uint16_t>(0);
		bitStream.Write<uint8_t>(0); //not mythran nametag
		bitStream.Write(LUWString(goonBName));
		bitStream.Write<uint8_t>(0); //not mythran for receiver
		bitStream.Write<uint8_t>(3); //new whisper
		bitStream.Write(LUWString(message, 512));

		SystemAddress sysAddr = goonB->sysAddr;
		SEND_PACKET;
	}
}

void ChatPacketHandler::HandleTeamInvite(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID;
	inStream.Read(playerID);
	std::string invitedPlayer = PacketUtils::ReadString(0x14, packet, true);

	auto* player = playerContainer.GetPlayerData(playerID);

	if (player == nullptr) {
		return;
	}

	auto* team = playerContainer.GetTeam(playerID);

	if (team == nullptr) {
		team = playerContainer.CreateTeam(playerID);
	}

	auto* other = playerContainer.GetPlayerData(invitedPlayer);

	if (other == nullptr) {
		return;
	}

	if (playerContainer.GetTeam(other->playerID) != nullptr) {
		return;
	}

	if (team->memberIDs.size() > 3) {
		// no more teams greater than 4

		LOG("Someone tried to invite a 5th player to a team");
		return;
	}

	SendTeamInvite(other, player);

	LOG("Got team invite: %llu -> %s", playerID, invitedPlayer.c_str());
}

void ChatPacketHandler::HandleTeamInviteResponse(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	uint32_t size = 0;
	inStream.Read(size);
	char declined = 0;
	inStream.Read(declined);
	LWOOBJID leaderID = LWOOBJID_EMPTY;
	inStream.Read(leaderID);

	LOG("Accepted invite: %llu -> %llu (%d)", playerID, leaderID, declined);

	if (declined) {
		return;
	}

	auto* team = playerContainer.GetTeam(leaderID);

	if (team == nullptr) {
		LOG("Failed to find team for leader (%llu)", leaderID);

		team = playerContainer.GetTeam(playerID);
	}

	if (team == nullptr) {
		LOG("Failed to find team for player (%llu)", playerID);
		return;
	}

	playerContainer.AddMember(team, playerID);
}

void ChatPacketHandler::HandleTeamLeave(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	uint32_t size = 0;
	inStream.Read(size);

	auto* team = playerContainer.GetTeam(playerID);

	LOG("(%llu) leaving team", playerID);

	if (team != nullptr) {
		playerContainer.RemoveMember(team, playerID, false, false, true);
	}
}

void ChatPacketHandler::HandleTeamKick(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);

	std::string kickedPlayer = PacketUtils::ReadString(0x14, packet, true);

	LOG("(%llu) kicking (%s) from team", playerID, kickedPlayer.c_str());

	auto* kicked = playerContainer.GetPlayerData(kickedPlayer);

	LWOOBJID kickedId = LWOOBJID_EMPTY;

	if (kicked != nullptr) {
		kickedId = kicked->playerID;
	} else {
		kickedId = playerContainer.GetId(GeneralUtils::UTF8ToUTF16(kickedPlayer));
	}

	if (kickedId == LWOOBJID_EMPTY) return;

	auto* team = playerContainer.GetTeam(playerID);

	if (team != nullptr) {
		if (team->leaderID != playerID || team->leaderID == kickedId) return;

		playerContainer.RemoveMember(team, kickedId, false, true, false);
	}
}

void ChatPacketHandler::HandleTeamPromote(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);

	std::string promotedPlayer = PacketUtils::ReadString(0x14, packet, true);

	LOG("(%llu) promoting (%s) to team leader", playerID, promotedPlayer.c_str());

	auto* promoted = playerContainer.GetPlayerData(promotedPlayer);

	if (promoted == nullptr) return;

	auto* team = playerContainer.GetTeam(playerID);

	if (team != nullptr) {
		if (team->leaderID != playerID) return;

		playerContainer.PromoteMember(team, promoted->playerID);
	}
}

void ChatPacketHandler::HandleTeamLootOption(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	uint32_t size = 0;
	inStream.Read(size);

	char option;
	inStream.Read(option);

	auto* team = playerContainer.GetTeam(playerID);

	if (team != nullptr) {
		if (team->leaderID != playerID) return;

		team->lootFlag = option;

		playerContainer.TeamStatusUpdate(team);

		playerContainer.UpdateTeamsOnWorld(team, false);
	}
}

void ChatPacketHandler::HandleTeamStatusRequest(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);

	auto* team = playerContainer.GetTeam(playerID);
	auto* data = playerContainer.GetPlayerData(playerID);

	if (team != nullptr && data != nullptr) {
		if (team->local && data->zoneID.GetMapID() != team->zoneId.GetMapID() && data->zoneID.GetCloneID() != team->zoneId.GetCloneID()) {
			playerContainer.RemoveMember(team, playerID, false, false, true, true);

			return;
		}

		if (team->memberIDs.size() <= 1 && !team->local) {
			playerContainer.DisbandTeam(team);

			return;
		}

		if (!team->local) {
			ChatPacketHandler::SendTeamSetLeader(data, team->leaderID);
		} else {
			ChatPacketHandler::SendTeamSetLeader(data, LWOOBJID_EMPTY);
		}

		playerContainer.TeamStatusUpdate(team);

		const auto leaderName = GeneralUtils::UTF8ToUTF16(data->playerName);

		for (const auto memberId : team->memberIDs) {
			auto* otherMember = playerContainer.GetPlayerData(memberId);

			if (memberId == playerID) continue;

			const auto memberName = playerContainer.GetName(memberId);

			if (otherMember != nullptr) {
				ChatPacketHandler::SendTeamSetOffWorldFlag(otherMember, data->playerID, data->zoneID);
			}
			ChatPacketHandler::SendTeamAddPlayer(data, false, team->local, false, memberId, memberName, otherMember != nullptr ? otherMember->zoneID : LWOZONEID(0, 0, 0));
		}

		playerContainer.UpdateTeamsOnWorld(team, false);
	}
}

void ChatPacketHandler::HandleGuildLeave(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	auto* player = playerContainer.GetPlayerData(playerID);
	if (!player) return;
	auto guild_id = Database::Get()->GetMembersGuild(playerID);
	Database::Get()->DeleteGuildMember(player->playerID);
	auto members = Database::Get()->GetGuildMembers(guild_id);
	if (members.empty()) Database::Get()->DeleteGuild(guild_id);

	// Send the removal, need to send this to all players in guild TODO
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(player->playerID);

	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::GUILD_REMOVE_PLAYER);
	bitStream.Write(eGuildLeaveReason::LEFT);
	bitStream.Write(LUWString(player->playerName));
	bitStream.Write(player->playerID);

	SystemAddress sysAddr = player->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::HandleGuildGetAll(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID playerID = LWOOBJID_EMPTY;
	inStream.Read(playerID);
	LOG("HandleGuildGetAll %llu", playerID);
	auto player = playerContainer.GetPlayerData(playerID);
	if (!player) return;
	auto guild_id = Database::Get()->GetMembersGuild(player->playerID);
	if (!guild_id) return;
	auto guild = Database::Get()->GetGuild(guild_id);
	if (!guild) return;
	auto members = Database::Get()->GetGuildMembers(guild_id);

	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(playerID);

	//portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::GUILD_DATA);
	bitStream.Write<uint8_t>(0);
	bitStream.Write(LUWString(guild->name, 31));
	bitStream.Write(LUWString("test1", 11));
	bitStream.Write(LUWString("test2", 11));
	bitStream.Write<uint32_t>(69);
	bitStream.Write<uint32_t>(0);
	bitStream.Write<uint32_t>(1);
	bitStream.Write<uint16_t>(0);
	constexpr int32_t sizeThing = 100;
	bitStream.Write<uint16_t>(sizeThing); // Size
	//Member data
	for (int i = 0; i < sizeThing; i++) {
		bitStream.Write<uint8_t>(1); // 0
		bitStream.Write<uint8_t>(1); // 1
		bitStream.Write<uint16_t>(1200); // 2
		bitStream.Write<uint16_t>(1200); // 4
		bitStream.Write<uint16_t>(1200); // 6
		bitStream.Write<uint16_t>(1200); // 4
		bitStream.Write<LWOOBJID>(player->playerID + i); // 8
		bitStream.Write(LUWString((i % 2 == 0 ? "abcdefghijklmnopqrstuvw" : "ABCDEFGHIJKLMNOPQRSTUVW"), 24)); // 16
		bitStream.Write<uint16_t>(0);
		bitStream.Write<LWOOBJID>(0);
		bitStream.Write<LWOOBJID>(0);
	}

	SystemAddress sysAddr = packet->systemAddress;
	SEND_PACKET;
	LOG("Send GUILD DATA");
	PacketUtils::SavePacket("GuildData.bin", (char*)bitStream.GetData(), bitStream.GetNumberOfBytesUsed());
}

void ChatPacketHandler::SendTeamInvite(PlayerData* receiver, PlayerData* sender) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::TEAM_INVITE);

	bitStream.Write(LUWString(sender->playerName.c_str()));
	bitStream.Write(sender->playerID);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamInviteConfirm(PlayerData* receiver, bool bLeaderIsFreeTrial, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, uint8_t ucResponseCode, std::u16string wsLeaderName) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER;

	bitStream.Write(receiver->playerID);
	bitStream.Write(eGameMessageType::TEAM_INVITE_CONFIRM);

	bitStream.Write(bLeaderIsFreeTrial);
	bitStream.Write(i64LeaderID);
	bitStream.Write(i64LeaderZoneID);
	bitStream.Write<uint32_t>(0); // BinaryBuffe, no clue what's in here
	bitStream.Write(ucLootFlag);
	bitStream.Write(ucNumOfOtherPlayers);
	bitStream.Write(ucResponseCode);
	bitStream.Write(static_cast<uint32_t>(wsLeaderName.size()));
	for (const auto character : wsLeaderName) {
		bitStream.Write(character);
	}

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamStatus(PlayerData* receiver, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, std::u16string wsLeaderName) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER;

	bitStream.Write(receiver->playerID);
	bitStream.Write(eGameMessageType::TEAM_GET_STATUS_RESPONSE);

	bitStream.Write(i64LeaderID);
	bitStream.Write(i64LeaderZoneID);
	bitStream.Write<uint32_t>(0); // BinaryBuffe, no clue what's in here
	bitStream.Write(ucLootFlag);
	bitStream.Write(ucNumOfOtherPlayers);
	bitStream.Write(static_cast<uint32_t>(wsLeaderName.size()));
	for (const auto character : wsLeaderName) {
		bitStream.Write(character);
	}

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamSetLeader(PlayerData* receiver, LWOOBJID i64PlayerID) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER;

	bitStream.Write(receiver->playerID);
	bitStream.Write(eGameMessageType::TEAM_SET_LEADER);

	bitStream.Write(i64PlayerID);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamAddPlayer(PlayerData* receiver, bool bIsFreeTrial, bool bLocal, bool bNoLootOnDeath, LWOOBJID i64PlayerID, std::u16string wsPlayerName, LWOZONEID zoneID) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER;

	bitStream.Write(receiver->playerID);
	bitStream.Write(eGameMessageType::TEAM_ADD_PLAYER);

	bitStream.Write(bIsFreeTrial);
	bitStream.Write(bLocal);
	bitStream.Write(bNoLootOnDeath);
	bitStream.Write(i64PlayerID);
	bitStream.Write(static_cast<uint32_t>(wsPlayerName.size()));
	for (const auto character : wsPlayerName) {
		bitStream.Write(character);
	}
	bitStream.Write1();
	if (receiver->zoneID.GetCloneID() == zoneID.GetCloneID()) {
		zoneID = LWOZONEID(zoneID.GetMapID(), zoneID.GetInstanceID(), 0);
	}
	bitStream.Write(zoneID);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamRemovePlayer(PlayerData* receiver, bool bDisband, bool bIsKicked, bool bIsLeaving, bool bLocal, LWOOBJID i64LeaderID, LWOOBJID i64PlayerID, std::u16string wsPlayerName) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER;

	bitStream.Write(receiver->playerID);
	bitStream.Write(eGameMessageType::TEAM_REMOVE_PLAYER);

	bitStream.Write(bDisband);
	bitStream.Write(bIsKicked);
	bitStream.Write(bIsLeaving);
	bitStream.Write(bLocal);
	bitStream.Write(i64LeaderID);
	bitStream.Write(i64PlayerID);
	bitStream.Write(static_cast<uint32_t>(wsPlayerName.size()));
	for (const auto character : wsPlayerName) {
		bitStream.Write(character);
	}

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendTeamSetOffWorldFlag(PlayerData* receiver, LWOOBJID i64PlayerID, LWOZONEID zoneID) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	CMSGHEADER;

	bitStream.Write(receiver->playerID);
	bitStream.Write(eGameMessageType::TEAM_SET_OFF_WORLD_FLAG);

	bitStream.Write(i64PlayerID);
	if (receiver->zoneID.GetCloneID() == zoneID.GetCloneID()) {
		zoneID = LWOZONEID(zoneID.GetMapID(), zoneID.GetInstanceID(), 0);
	}
	bitStream.Write(zoneID);

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendFriendUpdate(PlayerData* friendData, PlayerData* playerData, uint8_t notifyType, uint8_t isBestFriend) {
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
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(friendData->playerID);

	//portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::UPDATE_FRIEND_NOTIFY);
	bitStream.Write<uint8_t>(notifyType);

	std::string playerName = playerData->playerName.c_str();

	bitStream.Write(LUWString(playerName));

	bitStream.Write(playerData->zoneID.GetMapID());
	bitStream.Write(playerData->zoneID.GetInstanceID());

	if (playerData->zoneID.GetCloneID() == friendData->zoneID.GetCloneID()) {
		bitStream.Write(0);
	} else {
		bitStream.Write(playerData->zoneID.GetCloneID());
	}

	bitStream.Write<uint8_t>(isBestFriend); //isBFF
	bitStream.Write<uint8_t>(0); //isFTP

	SystemAddress sysAddr = friendData->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendFriendRequest(PlayerData* receiver, PlayerData* sender) {
	if (!receiver || !sender) return;

	//Make sure people aren't requesting people that they're already friends with:
	for (auto fr : receiver->friends) {
		if (fr.friendID == sender->playerID) {
			SendFriendResponse(sender, receiver, eAddFriendResponseType::ALREADYFRIEND, fr.isBestFriend);
			return; //we have this player as a friend, yeet this function so it doesn't send another request.
		}
	}

	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::ADD_FRIEND_REQUEST);
	bitStream.Write(LUWString(sender->playerName.c_str()));
	bitStream.Write<uint8_t>(0); // This is a BFF flag however this is unused in live and does not have an implementation client side.

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendFriendResponse(PlayerData* receiver, PlayerData* sender, eAddFriendResponseType responseCode, uint8_t isBestFriendsAlready, uint8_t isBestFriendRequest) {
	if (!receiver || !sender) return;

	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	// Portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::ADD_FRIEND_RESPONSE);
	bitStream.Write(responseCode);
	// For all requests besides accepted, write a flag that says whether or not we are already best friends with the receiver.
	bitStream.Write<uint8_t>(responseCode != eAddFriendResponseType::ACCEPTED ? isBestFriendsAlready : sender->sysAddr != UNASSIGNED_SYSTEM_ADDRESS);
	// Then write the player name
	bitStream.Write(LUWString(sender->playerName.c_str()));
	// Then if this is an acceptance code, write the following extra info.
	if (responseCode == eAddFriendResponseType::ACCEPTED) {
		bitStream.Write(sender->playerID);
		bitStream.Write(sender->zoneID);
		bitStream.Write(isBestFriendRequest); //isBFF
		bitStream.Write<uint8_t>(0); //isFTP
	}
	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}

void ChatPacketHandler::SendRemoveFriend(PlayerData* receiver, std::string& personToRemove, bool isSuccessful) {
	if (!receiver) return;

	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT_INTERNAL, eChatInternalMessageType::ROUTE_TO_PLAYER);
	bitStream.Write(receiver->playerID);

	//portion that will get routed:
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::REMOVE_FRIEND_RESPONSE);
	bitStream.Write<uint8_t>(isSuccessful); //isOnline
	bitStream.Write(LUWString(personToRemove));

	SystemAddress sysAddr = receiver->sysAddr;
	SEND_PACKET;
}
