#ifndef CHATPACKETSLEGACY_H
#define CHATPACKETSLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written chat packet code that dNet/ChatPackets.h replaced (dNet/ChatPackets.cpp,
// dChatServer/{ChatPacketHandler,ChatIgnoreList,TeamContainer,PlayerContainer}.cpp, dWorldServer/WorldServer.cpp and
// the dGame senders at f8f977f8). Only the namespace changed, except:
//  - senders that wrote to Game::chatServer (a RakPeer the tests don't have) write into `bitStream` instead;
//  - the parts of handlers that write or read a packet are copied on their own, with the state they used passed in;
//  - old readers return what they read in a struct instead of acting on it.
// The byte-equality tests send the same inputs through these and through the new structs and require identical
// bytes, so the wire format is pinned even after the production code is deleted.

#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "Game.h"
#include "MessageType/Chat.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "ServiceType.h"
#include "PlayerContainer.h"
#include "eAddFriendResponseCode.h"
#include "eAddFriendResponseType.h"
#include "eChatChannel.h"
#include "eChatMessageResponseCode.h"
#include "eGameMasterLevel.h"

#include <map>
#include <string>
#include <vector>

namespace LegacyChat {
	// dNet/ChatPackets.h
	struct ShowAllRequest{
		LWOOBJID requestor = LWOOBJID_EMPTY;
		bool displayZoneData = true;
		bool displayIndividualPlayers = true;
		void Serialize(RakNet::BitStream& bitStream);
		void Deserialize(RakNet::BitStream& inStream);
	};

	struct FindPlayerRequest{
		LWOOBJID requestor = LWOOBJID_EMPTY;
		LUWString playerName;
		void Serialize(RakNet::BitStream& bitStream);
		void Deserialize(RakNet::BitStream& inStream);
	};

	// dNet/ChatPackets.cpp
	inline void ShowAllRequest::Serialize(RakNet::BitStream& bitStream) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::SHOW_ALL);
		bitStream.Write(this->requestor);
		bitStream.Write(this->displayZoneData);
		bitStream.Write(this->displayIndividualPlayers);
	}

	inline void ShowAllRequest::Deserialize(RakNet::BitStream& inStream) {
		inStream.Read(this->requestor);
		inStream.Read(this->displayZoneData);
		inStream.Read(this->displayIndividualPlayers);
	}

	inline void FindPlayerRequest::Serialize(RakNet::BitStream& bitStream) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WHO);
		bitStream.Write(this->requestor);
		bitStream.Write(this->playerName);
	}

	inline void FindPlayerRequest::Deserialize(RakNet::BitStream& inStream) {
		inStream.Read(this->requestor);
		inStream.Read(this->playerName);
	}

	inline void SendChatMessage(const SystemAddress& sysAddr, char chatChannel, const std::string& senderName, LWOOBJID playerObjectID, bool senderMythran, const std::u16string& message) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::GENERAL_CHAT_MESSAGE);

		bitStream.Write<uint64_t>(0);
		bitStream.Write(chatChannel);

		bitStream.Write<uint32_t>(message.size());
		bitStream.Write(LUWString(senderName));

		bitStream.Write(playerObjectID);
		bitStream.Write<uint16_t>(0);
		bitStream.Write<char>(0);

		for (uint32_t i = 0; i < message.size(); ++i) {
			bitStream.Write<uint16_t>(message[i]);
		}
		bitStream.Write<uint16_t>(0);

		SEND_PACKET_BROADCAST;
	}

	inline void SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, const bool broadcast = false) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::GENERAL_CHAT_MESSAGE);

		bitStream.Write<uint64_t>(0);
		bitStream.Write<char>(4);

		bitStream.Write<uint32_t>(message.size());
		bitStream.Write(LUWString("", 33));

		bitStream.Write<uint64_t>(0);
		bitStream.Write<uint16_t>(0);
		bitStream.Write<char>(0);

		for (uint32_t i = 0; i < message.size(); ++i) {
			bitStream.Write<uint16_t>(message[i]);
		}

		bitStream.Write<uint16_t>(0);

		//This is so Wincent's announcement works:
		if (sysAddr != UNASSIGNED_SYSTEM_ADDRESS) {
			SEND_PACKET;
			return;
		}

		SEND_PACKET_BROADCAST;
	}

	inline void SendSystemMessage(const SystemAddress& sysAddr, const std::string& message, const bool broadcast = false) {
		LegacyChat::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message), broadcast);
	}

	inline void SendMessageFail(const SystemAddress& sysAddr) {
		//0x00 - "Chat is currently disabled."
		//0x01 - "Upgrade to a full LEGO Universe Membership to chat with other players."

		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::SEND_CANNED_TEXT);
		bitStream.Write<uint8_t>(0); //response type, options above ^
		//docs say there's a wstring here-- no idea what it's for, or if it's even needed so leaving it as is for now.
		SEND_PACKET;
	}

	// ChatPackets::Announcement::Serialize, with its header
	inline void WriteAnnouncement(RakNet::BitStream& bitStream, const std::string& title, const std::string& message) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::GM_ANNOUNCE);
		bitStream.Write<uint32_t>(title.size());
		bitStream.Write(title);
		bitStream.Write<uint32_t>(message.size());
		bitStream.Write(message);
	}

	// ChatPackets::TeamInviteInitialResponse::Serialize, with its header
	inline void WriteTeamInviteInitialResponse(RakNet::BitStream& bitstream, bool inviteFailedToSend, const LUWString& playerName) {
		BitStreamUtils::WriteHeader(bitstream, ServiceType::CLIENT, MessageType::Client::TEAM_INVITE_INITIAL_RESPONSE);
		bitstream.Write<uint8_t>(inviteFailedToSend);
		bitstream.Write(playerName);
	}

	// ChatPackets::SendRoutedMsg, with the routed message's bytes (msg.WritePacket) given
	inline void SendRoutedMsg(RakNet::BitStream& msgPacket, const LWOOBJID targetID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(targetID);

		// Now write the actual packet
		bitStream.WriteBits(msgPacket.GetData(), msgPacket.GetNumberOfBitsUsed());
		Game::server->Send(bitStream, sysAddr, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);
	}

	// dChatServer/ChatPacketHandler.cpp

	// The end of HandleFriendlistRequest
	inline void SendFriendsList(const LWOOBJID playerID, const PlayerData& player) {
		//Now, we need to send the friendlist to the server they came from:
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(playerID);

		//portion that will get routed:
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::GET_FRIENDS_LIST_RESPONSE);
		bitStream.Write<uint8_t>(0);
		bitStream.Write<uint16_t>(1); //Length of packet -- just writing one as it doesn't matter, client skips it.
		bitStream.Write<uint16_t>(player.friends.size());

		for (const auto& data : player.friends) {
			data.Serialize(bitStream);
		}

		SystemAddress sysAddr = player.worldServerSysAddr;
		SEND_PACKET;
	}

	// The end of HandleWho
	inline void SendWhoResponse(const FindPlayerRequest& request, const PlayerData& sender, const PlayerData& player) {
		bool online = player;

		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(request.requestor);

		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::WHO_RESPONSE);
		bitStream.Write<uint8_t>(online);
		bitStream.Write(player.zoneID.GetMapID());
		bitStream.Write(player.zoneID.GetInstanceID());
		bitStream.Write(player.zoneID.GetCloneID());
		bitStream.Write(request.playerName);

		SystemAddress sysAddr = sender.worldServerSysAddr;
		SEND_PACKET;
	}

	// The end of HandleShowAll, with Game::playerContainer's counts and players passed in
	inline void SendShowAllResponse(const ShowAllRequest& request, const PlayerData& sender, uint32_t playerCount, uint32_t simCount, const std::map<LWOOBJID, PlayerData>& allPlayers) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(request.requestor);

		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::SHOW_ALL_RESPONSE);
		bitStream.Write<uint8_t>(!request.displayZoneData && !request.displayIndividualPlayers);
		bitStream.Write(playerCount);
		bitStream.Write(simCount);
		bitStream.Write<uint8_t>(request.displayIndividualPlayers);
		bitStream.Write<uint8_t>(request.displayZoneData);
		if (request.displayZoneData || request.displayIndividualPlayers) {
			for (auto& [playerID, playerData] : allPlayers) {
				if (!playerData) continue;
				bitStream.Write<uint8_t>(0); // structure packing
				if (request.displayIndividualPlayers) bitStream.Write(LUWString(playerData.playerName));
				if (request.displayZoneData) {
					bitStream.Write(playerData.zoneID.GetMapID());
					bitStream.Write(playerData.zoneID.GetInstanceID());
					bitStream.Write(playerData.zoneID.GetCloneID());
				}
			}
		}
		SystemAddress sysAddr = sender.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendPrivateChatMessage(const PlayerData& sender, const PlayerData& receiver, const PlayerData& routeTo, const LUWString& message, const eChatChannel channel, const eChatMessageResponseCode responseCode) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(routeTo.playerID);

		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::PRIVATE_CHAT_MESSAGE);
		bitStream.Write(sender.playerID);
		bitStream.Write(channel);
		bitStream.Write<uint32_t>(0); // not used
		bitStream.Write(LUWString(sender.playerName));
		bitStream.Write(sender.playerID);
		bitStream.Write<uint16_t>(0); // sourceID
		bitStream.Write(sender.gmLevel);
		bitStream.Write(LUWString(receiver.playerName));
		bitStream.Write(receiver.gmLevel);
		bitStream.Write(responseCode);
		bitStream.Write(message);

		SystemAddress sysAddr = routeTo.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendFriendUpdate(const PlayerData& friendData, const PlayerData& playerData, uint8_t notifyType, uint8_t isBestFriend) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(friendData.playerID);

		//portion that will get routed:
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::UPDATE_FRIEND_NOTIFY);
		bitStream.Write<uint8_t>(notifyType);

		std::string playerName = playerData.playerName.c_str();

		bitStream.Write(LUWString(playerName));

		bitStream.Write(playerData.zoneID.GetMapID());
		bitStream.Write(playerData.zoneID.GetInstanceID());

		if (playerData.zoneID.GetCloneID() == friendData.zoneID.GetCloneID()) {
			bitStream.Write(0);
		} else {
			bitStream.Write(playerData.zoneID.GetCloneID());
		}

		bitStream.Write<uint8_t>(isBestFriend); //isBFF
		bitStream.Write<uint8_t>(0); //isFTP

		SystemAddress sysAddr = friendData.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendFriendResponse(const PlayerData& receiver, const PlayerData& sender, eAddFriendResponseType responseCode, uint8_t isBestFriendsAlready = 0U, uint8_t isBestFriendRequest = 0U) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		// Portion that will get routed:
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::ADD_FRIEND_RESPONSE);
		bitStream.Write(responseCode);
		// For all requests besides accepted, write a flag that says whether or not we are already best friends with the receiver.
		bitStream.Write<uint8_t>(responseCode != eAddFriendResponseType::ACCEPTED ? isBestFriendsAlready : sender.worldServerSysAddr != UNASSIGNED_SYSTEM_ADDRESS);
		// Then write the player name
		bitStream.Write(LUWString(sender.playerName));
		// Then if this is an acceptance code, write the following extra info.
		if (responseCode == eAddFriendResponseType::ACCEPTED) {
			bitStream.Write(sender.playerID);
			bitStream.Write(sender.zoneID);
			bitStream.Write(isBestFriendRequest); //isBFF
			bitStream.Write<uint8_t>(0); //isFTP
		}
		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendFriendRequest(const PlayerData& receiver, const PlayerData& sender) {
		//Make sure people aren't requesting people that they're already friends with:
		for (const auto& fr : receiver.friends) {
			if (fr.friendID == sender.playerID) {
				SendFriendResponse(sender, receiver, eAddFriendResponseType::ALREADYFRIEND, fr.isBestFriend);
				return; //we have this player as a friend, yeet this function so it doesn't send another request.
			}
		}

		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		//portion that will get routed:
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::ADD_FRIEND_REQUEST);
		bitStream.Write(LUWString(sender.playerName));
		bitStream.Write<uint8_t>(0); // This is a BFF flag however this is unused in live and does not have an implementation client side.

		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendRemoveFriend(const PlayerData& receiver, std::string& personToRemove, bool isSuccessful) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		//portion that will get routed:
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::REMOVE_FRIEND_RESPONSE);
		bitStream.Write<uint8_t>(isSuccessful); //isOnline
		bitStream.Write(LUWString(personToRemove));

		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	// dChatServer/ChatIgnoreList.cpp
	enum class AddResponse : uint8_t {
		SUCCESS,
		ALREADY_IGNORED,
		PLAYER_NOT_FOUND,
		GENERAL_ERROR,
	};

	inline void WriteOutgoingReplyHeader(RakNet::BitStream& bitStream, const LWOOBJID& receivingPlayer, const MessageType::Client type) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receivingPlayer);

		//portion that will get routed:
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, type);
	}

	// The end of GetIgnoreList
	inline void SendIgnoreList(const PlayerData& receiver, const SystemAddress& packetSystemAddress) {
		CBITSTREAM;
		WriteOutgoingReplyHeader(bitStream, receiver.playerID, MessageType::Client::GET_IGNORE_LIST_RESPONSE);

		bitStream.Write<uint8_t>(false); // Is Free Trial, but we don't care about that
		bitStream.Write<uint16_t>(0); // literally spacing due to struct alignment

		bitStream.Write<uint16_t>(receiver.ignoredPlayers.size());
		for (const auto& ignoredPlayer : receiver.ignoredPlayers) {
			bitStream.Write(ignoredPlayer.playerId);
			bitStream.Write(LUWString(ignoredPlayer.playerName, 36));
		}

		Game::server->Send(bitStream, packetSystemAddress, false);
	}

	// The response AddIgnore writes, with the outcome passed in
	inline void SendAddIgnoreResponse(const PlayerData& receiver, AddResponse response, const std::string& toIgnoreStr, LWOOBJID ignoredPlayerId, const SystemAddress& packetSystemAddress) {
		CBITSTREAM;
		WriteOutgoingReplyHeader(bitStream, receiver.playerID, MessageType::Client::ADD_IGNORE_RESPONSE);

		bitStream.Write(response);

		LUWString playerNameSend(toIgnoreStr, 33);
		bitStream.Write(playerNameSend);
		bitStream.Write(ignoredPlayerId);

		Game::server->Send(bitStream, packetSystemAddress, false);
	}

	// The end of RemoveIgnore
	inline void SendRemoveIgnoreResponse(const PlayerData& receiver, const std::string& removedIgnoreStr, const SystemAddress& packetSystemAddress) {
		CBITSTREAM;
		WriteOutgoingReplyHeader(bitStream, receiver.playerID, MessageType::Client::REMOVE_IGNORE_RESPONSE);

		bitStream.Write<int8_t>(0);
		LUWString playerNameSend(removedIgnoreStr, 33);
		bitStream.Write(playerNameSend);

		Game::server->Send(bitStream, packetSystemAddress, false);
	}

	// dChatServer/TeamContainer.cpp
	inline void SendTeamInvite(const PlayerData& receiver, const PlayerData& sender) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		//portion that will get routed:
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::TEAM_INVITE);

		bitStream.Write(LUWString(sender.playerName.c_str()));
		bitStream.Write(sender.playerID);

		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendTeamInviteConfirm(const PlayerData& receiver, bool bLeaderIsFreeTrial, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, uint8_t ucResponseCode, std::u16string wsLeaderName) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		//portion that will get routed:
		CMSGHEADER;

		bitStream.Write(receiver.playerID);
		bitStream.Write(MessageType::Game::TEAM_INVITE_CONFIRM);

		bitStream.Write(bLeaderIsFreeTrial);
		bitStream.Write(i64LeaderID);
		bitStream.Write(i64LeaderZoneID);
		bitStream.Write<uint32_t>(0); // BinaryBuffe, no clue what's in here
		bitStream.Write(ucLootFlag);
		bitStream.Write(ucNumOfOtherPlayers);
		bitStream.Write(ucResponseCode);
		bitStream.Write<uint32_t>(wsLeaderName.size());
		for (const auto character : wsLeaderName) {
			bitStream.Write(character);
		}

		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendTeamStatus(const PlayerData& receiver, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, std::u16string wsLeaderName) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		//portion that will get routed:
		CMSGHEADER;

		bitStream.Write(receiver.playerID);
		bitStream.Write(MessageType::Game::TEAM_GET_STATUS_RESPONSE);

		bitStream.Write(i64LeaderID);
		bitStream.Write(i64LeaderZoneID);
		bitStream.Write<uint32_t>(0); // BinaryBuffe, no clue what's in here
		bitStream.Write(ucLootFlag);
		bitStream.Write(ucNumOfOtherPlayers);
		bitStream.Write<uint32_t>(wsLeaderName.size());
		for (const auto character : wsLeaderName) {
			bitStream.Write(character);
		}

		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendTeamSetLeader(const PlayerData& receiver, LWOOBJID i64PlayerID) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		//portion that will get routed:
		CMSGHEADER;

		bitStream.Write(receiver.playerID);
		bitStream.Write(MessageType::Game::TEAM_SET_LEADER);

		bitStream.Write(i64PlayerID);

		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendTeamAddPlayer(const PlayerData& receiver, bool bIsFreeTrial, bool bLocal, bool bNoLootOnDeath, LWOOBJID i64PlayerID, std::u16string wsPlayerName, LWOZONEID zoneID) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		//portion that will get routed:
		CMSGHEADER;

		bitStream.Write(receiver.playerID);
		bitStream.Write(MessageType::Game::TEAM_ADD_PLAYER);

		bitStream.Write(bIsFreeTrial);
		bitStream.Write(bLocal);
		bitStream.Write(bNoLootOnDeath);
		bitStream.Write(i64PlayerID);
		bitStream.Write<uint32_t>(wsPlayerName.size());
		for (const auto character : wsPlayerName) {
			bitStream.Write(character);
		}
		bitStream.Write1();
		if (receiver.zoneID.GetCloneID() == zoneID.GetCloneID()) {
			zoneID = LWOZONEID(zoneID.GetMapID(), zoneID.GetInstanceID(), 0);
		}
		bitStream.Write(zoneID);

		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendTeamRemovePlayer(const PlayerData& receiver, bool bDisband, bool bIsKicked, bool bIsLeaving, bool bLocal, LWOOBJID i64LeaderID, LWOOBJID i64PlayerID, std::u16string wsPlayerName) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		//portion that will get routed:
		CMSGHEADER;

		bitStream.Write(receiver.playerID);
		bitStream.Write(MessageType::Game::TEAM_REMOVE_PLAYER);

		bitStream.Write(bDisband);
		bitStream.Write(bIsKicked);
		bitStream.Write(bIsLeaving);
		bitStream.Write(bLocal);
		bitStream.Write(i64LeaderID);
		bitStream.Write(i64PlayerID);
		bitStream.Write<uint32_t>(wsPlayerName.size());
		for (const auto character : wsPlayerName) {
			bitStream.Write(character);
		}

		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	inline void SendTeamSetOffWorldFlag(const PlayerData& receiver, LWOOBJID i64PlayerID, LWOZONEID zoneID) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
		bitStream.Write(receiver.playerID);

		//portion that will get routed:
		CMSGHEADER;

		bitStream.Write(receiver.playerID);
		bitStream.Write(MessageType::Game::TEAM_SET_OFF_WORLD_FLAG);

		bitStream.Write(i64PlayerID);
		if (receiver.zoneID.GetCloneID() == zoneID.GetCloneID()) {
			zoneID = LWOZONEID(zoneID.GetMapID(), zoneID.GetInstanceID(), 0);
		}
		bitStream.Write(zoneID);

		SystemAddress sysAddr = receiver.worldServerSysAddr;
		SEND_PACKET;
	}

	// UpdateTeamsOnWorld, with the team's fields passed in
	inline void UpdateTeamsOnWorld(LWOOBJID teamID, uint8_t lootFlag, const std::vector<LWOOBJID>& memberIDs, bool deleteTeam) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::TEAM_GET_STATUS);

		bitStream.Write(teamID);
		bitStream.Write(deleteTeam);

		if (!deleteTeam) {
			bitStream.Write(lootFlag);
			bitStream.Write<char>(memberIDs.size());
			for (const auto memberID : memberIDs) {
				bitStream.Write(memberID);
			}
		}

		Game::server->Send(bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
	}

	// dChatServer/PlayerContainer.cpp
	inline void BroadcastMuteUpdate(LWOOBJID player, time_t time) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::GM_MUTE);

		bitStream.Write(player);
		bitStream.Write(time);

		Game::server->Send(bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
	}

	// World -> chat senders (they wrote to Game::chatServer; here into bitStream)

	// dWorldServer/WorldServer.cpp, LoadPlayer
	inline void WriteLoginSessionNotify(RakNet::BitStream& bitStream, LWOOBJID objectID, const std::string& playerName, const LWOZONEID& zone, time_t muteExpire, eGameMasterLevel gmLevel) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::LOGIN_SESSION_NOTIFY);
		bitStream.Write(objectID);
		bitStream.Write<uint32_t>(playerName.size());
		for (size_t i = 0; i < playerName.size(); i++) {
			bitStream.Write(playerName[i]);
		}

		bitStream.Write(zone.GetMapID());
		bitStream.Write(zone.GetInstanceID());
		bitStream.Write(zone.GetCloneID());
		bitStream.Write(muteExpire);
		bitStream.Write(gmLevel);
	}

	// dWorldServer/WorldServer.cpp and dGame/UserManager.cpp
	inline void WriteUnexpectedDisconnect(RakNet::BitStream& bitStream, LWOOBJID objectID) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::UNEXPECTED_DISCONNECT);
		bitStream.Write(objectID);
	}

	// dGame/Entity.cpp, SetGMLevel
	inline void WriteGMLevelUpdate(RakNet::BitStream& bitStream, LWOOBJID m_ObjectID, eGameMasterLevel m_GMLevel) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::GMLEVEL_UPDATE);
		bitStream.Write(m_ObjectID);
		bitStream.Write(m_GMLevel);
	}

	// dGame/User.cpp and the /mute command
	inline void WriteGMMute(RakNet::BitStream& bitStream, LWOOBJID characterId, time_t expire) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::GM_MUTE);

		bitStream.Write(characterId);
		bitStream.Write(expire);
	}

	// dGame/dComponents/ActivityComponent.cpp, ActivityInstance::StartZone
	inline void WriteCreateTeam(RakNet::BitStream& bitStream, LWOOBJID leaderID, const std::vector<LWOOBJID>& m_Participants, LWOZONEID zoneId) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::CREATE_TEAM);

		bitStream.Write(leaderID);
		bitStream.Write(m_Participants.size());

		for (const auto& participant : m_Participants) {
			bitStream.Write(participant);
		}

		bitStream.Write(zoneId);
	}

	// dGame/dUtilities/SlashCommandHandler.cpp, SendAnnouncement
	inline void WriteGMAnnounce(RakNet::BitStream& bitStream, const std::string& title, const std::string& message) {
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, MessageType::Chat::GM_ANNOUNCE);

		bitStream.Write<uint32_t>(title.size());
		for (auto character : title) {
			bitStream.Write<char>(character);
		}

		bitStream.Write<uint32_t>(message.size());
		for (auto character : message) {
			bitStream.Write<char>(character);
		}
	}

	// Readers. What the old handlers read, in order; the stream is past the packet header.

	struct PlayerAndName {
		LWOOBJID playerID{};
		LUWString name;
		char extra{};
	};

	// HandleFriendRequest
	inline PlayerAndName ReadFriendRequest(RakNet::BitStream& inStream) {
		LWOOBJID requestorPlayerID;
		LUWString LUplayerName;
		char isBestFriendRequest{};

		inStream.Read(requestorPlayerID);
		inStream.IgnoreBytes(4);
		inStream.Read(LUplayerName);
		inStream.Read(isBestFriendRequest);
		return { requestorPlayerID, LUplayerName, isBestFriendRequest };
	}

	// HandleFriendResponse
	inline PlayerAndName ReadFriendResponse(RakNet::BitStream& inStream) {
		LWOOBJID playerID;
		eAddFriendResponseCode clientResponseCode;
		LUWString friendName;

		inStream.Read(playerID);
		inStream.IgnoreBytes(4);
		inStream.Read(clientResponseCode);
		inStream.Read(friendName);
		return { playerID, friendName, static_cast<char>(clientResponseCode) };
	}

	// HandleRemoveFriend, AddIgnore, RemoveIgnore, HandleTeamInvite, HandleTeamKick, HandleTeamPromote
	inline PlayerAndName ReadPlayerAndName(RakNet::BitStream& inStream) {
		LWOOBJID playerID;
		LUWString LUFriendName;
		inStream.Read(playerID);
		inStream.IgnoreBytes(4);
		inStream.Read(LUFriendName);
		return { playerID, LUFriendName, 0 };
	}

	struct ChatRead {
		LWOOBJID playerID{};
		eChatChannel channel{};
		uint32_t size{};
		LUWString receiverName;
		LUWString message;
	};

	// HandleChatMessage (without the sender check)
	inline ChatRead ReadChatMessage(RakNet::BitStream& inStream) {
		LWOOBJID playerID;
		inStream.Read(playerID);

		eChatChannel channel;
		uint32_t size;

		inStream.IgnoreBytes(4);
		inStream.Read(channel);
		inStream.Read(size);

		inStream.IgnoreBytes(77);

		LUWString message(size);
		inStream.Read(message);
		return { playerID, channel, size, LUWString(), message };
	}

	// HandlePrivateChatMessage (without the sender check)
	inline ChatRead ReadPrivateChatMessage(RakNet::BitStream& inStream) {
		LWOOBJID playerID;
		inStream.Read(playerID);

		eChatChannel channel;
		uint32_t size;
		LUWString LUReceiverName;

		inStream.IgnoreBytes(4);
		inStream.Read(channel);

		inStream.Read(size);

		inStream.IgnoreBytes(77);

		inStream.Read(LUReceiverName);
		inStream.IgnoreBytes(2);

		LUWString message(size);
		inStream.Read(message);
		return { playerID, channel, size, LUReceiverName, message };
	}

	struct TeamInviteResponseRead {
		LWOOBJID playerID{};
		char declined{};
		LWOOBJID leaderID{};
	};

	// HandleTeamInviteResponse
	inline TeamInviteResponseRead ReadTeamInviteResponse(RakNet::BitStream& inStream) {
		LWOOBJID playerID = LWOOBJID_EMPTY;
		inStream.Read(playerID);
		uint32_t size = 0;
		inStream.Read(size);
		char declined = 0;
		inStream.Read(declined);
		LWOOBJID leaderID = LWOOBJID_EMPTY;
		inStream.Read(leaderID);
		return { playerID, declined, leaderID };
	}

	// HandleTeamLootOption
	inline std::pair<LWOOBJID, char> ReadTeamLootOption(RakNet::BitStream& inStream) {
		LWOOBJID playerID = LWOOBJID_EMPTY;
		inStream.Read(playerID);
		uint32_t size = 0;
		inStream.Read(size);

		char option;
		inStream.Read(option);
		return { playerID, option };
	}

	struct CreateTeamRead {
		LWOOBJID playerID{};
		std::vector<LWOOBJID> members;
		LWOZONEID zoneId;
	};

	// TeamContainer::CreateTeamServer (without the size check)
	inline CreateTeamRead ReadCreateTeam(RakNet::BitStream& inStream) {
		LWOOBJID playerID;
		inStream.Read(playerID);
		size_t membersSize = 0;
		inStream.Read(membersSize);

		std::vector<LWOOBJID> members;

		members.reserve(membersSize);

		for (size_t i = 0; i < membersSize; i++) {
			LWOOBJID member;
			inStream.Read(member);
			members.push_back(member);
		}

		LWOZONEID zoneId;

		inStream.Read(zoneId);
		return { playerID, members, zoneId };
	}

	// PlayerContainer::InsertPlayer (into a PlayerData)
	inline bool ReadLoginSessionNotify(RakNet::BitStream& inStream, PlayerData& data) {
		LWOOBJID playerId;
		if (!inStream.Read(playerId)) {
			return false;
		}
		data.playerID = playerId;

		uint32_t len;
		if (!inStream.Read<uint32_t>(len)) return false;

		if (len > 33) {
			return false;
		}

		data.playerName.resize(len);
		inStream.ReadAlignedBytes(reinterpret_cast<unsigned char*>(data.playerName.data()), len);

		if (!inStream.Read(data.zoneID)) return false;
		if (!inStream.Read(data.muteExpire)) return false;
		if (!inStream.Read(data.gmLevel)) return false;
		return true;
	}

	// dWorldServer/WorldServer.cpp, HandlePacketChat GM_ANNOUNCE
	inline std::pair<std::string, std::string> ReadGMAnnounce(RakNet::BitStream& inStream) {
		std::string title;
		std::string msg;

		uint32_t len;
		inStream.Read<uint32_t>(len);
		for (uint32_t i = 0; len > i; i++) {
			char character;
			inStream.Read<char>(character);
			title += character;
		}

		len = 0;
		inStream.Read<uint32_t>(len);
		for (uint32_t i = 0; len > i; i++) {
			char character;
			inStream.Read<char>(character);
			msg += character;
		}
		return { title, msg };
	}

	struct TeamStatusRead {
		LWOOBJID teamID = 0;
		bool deleteTeam{};
		char lootOption = 0;
		std::vector<LWOOBJID> members;
	};

	// dWorldServer/WorldServer.cpp, HandlePacketChat TEAM_GET_STATUS
	inline TeamStatusRead ReadTeamStatus(RakNet::BitStream& inStream) {
		LWOOBJID teamID = 0;
		char lootOption = 0;
		char memberCount = 0;
		std::vector<LWOOBJID> members;

		inStream.Read(teamID);
		bool deleteTeam = inStream.ReadBit();

		if (deleteTeam) {
			return { teamID, deleteTeam, 0, {} };
		}

		inStream.Read(lootOption);
		inStream.Read(memberCount);
		for (char i = 0; i < memberCount; i++) {
			LWOOBJID member = LWOOBJID_EMPTY;
			inStream.Read(member);
			members.push_back(member);
		}
		return { teamID, deleteTeam, lootOption, members };
	}

	// dWorldServer/WorldServer.cpp, HandlePacketChat WORLD_ROUTE_PACKET: the bytes sent on to the player
	inline std::pair<LWOOBJID, std::vector<uint8_t>> ReadWorldRoute(RakNet::BitStream& inStream) {
		LWOOBJID playerID;
		inStream.Read(playerID);

		std::vector<uint8_t> out;
		unsigned char data;
		while (inStream.Read(data)) {
			out.push_back(data);
		}
		return { playerID, out };
	}
}

#endif // CHATPACKETSLEGACY_H
