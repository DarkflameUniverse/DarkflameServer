#include "ChatPacketHandler.h"
#include "PlayerContainer.h"
#include "Database.h"
#include <ctime>
#include <vector>
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
#include "ServiceType.h"
#include "MessageType/Chat.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "StringifiedEnum.h"
#include "eGameMasterLevel.h"
#include "ChatPackets.h"
#include "TeamContainer.h"

void ChatPacketHandler::SendRouted(const LWOOBJID target, const SystemAddress& world, const LUBitStream& msg, const bool broadcast) {
	ChatPackets::WorldRoutePacket route;
	route.targetID = target;
	route.routed = &msg;

	RakNet::BitStream bitStream;
	route.WritePacket(bitStream);
	Game::server->Send(bitStream, world, broadcast);
}

void ChatPacketHandler::HandleFriendlistRequest(const ChatPackets::GetFriendsList& request, const SystemAddress& sysAddr) {
	//Get from the packet which player we want to do something with:
	const LWOOBJID playerID = request.playerID;

	auto& player = Game::playerContainer.GetPlayerDataMutable(playerID);
	if (!player) return;

	auto friendsList = Database::Get()->GetFriendsList(playerID);
	for (const auto& friendData : friendsList) {
		FriendData fd;
		fd.isFTP = false; // not a thing in DLU
		fd.friendID = friendData.friendID;
		GeneralUtils::SetBit(fd.friendID, eObjectBits::CHARACTER);

		fd.isBestFriend = friendData.isBestFriend; //0 = friends, 1 = left_requested, 2 = right_requested, 3 = both_accepted - are now bffs
		if (fd.isBestFriend) player.countOfBestFriends += 1;
		fd.friendName = friendData.friendName;

		//Now check if they're online:
		const auto& fr = Game::playerContainer.GetPlayerData(fd.friendID);

		if (fr) {
			fd.isOnline = true;
			fd.zoneID = fr.zoneID;

			//Since this friend is online, we need to update them on the fact that we've just logged in:
			if (player.isLogin) SendFriendUpdate(fr, player, 1, fd.isBestFriend);
		} else {
			fd.isOnline = false;
			fd.zoneID = LWOZONEID();
		}

		player.friends.push_back(fd);
	}

	//Now, we need to send the friendlist to the server they came from:
	ClientPackets::GetFriendsListResponse response;
	response.friends = player.friends;
	SendRouted(playerID, player.worldServerSysAddr, response);
}

void ChatPacketHandler::HandleFriendRequest(const ChatPackets::AddFriendRequest& request, const SystemAddress& sysAddr) {
	const LWOOBJID requestorPlayerID = request.playerID;
	const char isBestFriendRequest = request.isBestFriendRequest;

	auto playerName = request.friendName.GetAsString();

	auto& requestor = Game::playerContainer.GetPlayerDataMutable(requestorPlayerID);
	if (!requestor) {
		LOG("No requestor player %llu sent to %s found.", requestorPlayerID, playerName.c_str());
		return;
	}

	// you cannot friend yourself
	if (requestor.playerName == playerName) {
		SendFriendResponse(requestor, requestor, eAddFriendResponseType::GENERALERROR);
		return;
	};

	// Intentional copy
	PlayerData requestee = Game::playerContainer.GetPlayerData(playerName);

	// Check if player is online first
	if (isBestFriendRequest && !requestee) {
		for (auto& friendDataCandidate : requestor.friends) {
			if (friendDataCandidate.friendName != playerName) continue;
			// Setup the needed info since you can add a best friend offline.
			requestee.playerID = friendDataCandidate.friendID;
			requestee.playerName = friendDataCandidate.friendName;
			requestee.zoneID = LWOZONEID();

			FriendData requesteeFriendData{};
			requesteeFriendData.friendID = requestor.playerID;
			requesteeFriendData.friendName = requestor.playerName;
			requesteeFriendData.isFTP = false;
			requesteeFriendData.isOnline = false;
			requesteeFriendData.zoneID = requestor.zoneID;
			requestee.friends.push_back(requesteeFriendData);
			requestee.worldServerSysAddr = UNASSIGNED_SYSTEM_ADDRESS;
			break;
		}
	}

	// If at this point we dont have a target, then they arent online and we cant send the request.
	// Send the response code that corresponds to what the error is.
	if (!requestee) {
		requestee.playerName = playerName;
		auto responseType = Database::Get()->GetCharacterInfo(playerName)
			? eAddFriendResponseType::NOTONLINE
			: eAddFriendResponseType::INVALIDCHARACTER;

		SendFriendResponse(requestor, requestee, responseType);
		return;
	}

	// Prevent GM friend spam
	// If the player we are trying to be friends with is not a civilian and we are a civilian, abort the process
	if (requestee.gmLevel > eGameMasterLevel::CIVILIAN && requestor.gmLevel == eGameMasterLevel::CIVILIAN) {
		SendFriendResponse(requestor, requestee, eAddFriendResponseType::MYTHRAN);
		return;
	}

	if (isBestFriendRequest) {

		uint8_t oldBestFriendStatus{};
		uint8_t bestFriendStatus{};
		auto bestFriendInfo = Database::Get()->GetBestFriendStatus(requestorPlayerID, requestee.playerID);
		if (bestFriendInfo) {
			// Get the IDs
			LWOOBJID queryPlayerID = bestFriendInfo->playerCharacterId;
			LWOOBJID queryFriendID = bestFriendInfo->friendCharacterId;
			oldBestFriendStatus = bestFriendInfo->bestFriendStatus;
			bestFriendStatus = oldBestFriendStatus;

			// Set the bits
			GeneralUtils::SetBit(queryPlayerID, eObjectBits::CHARACTER);
			GeneralUtils::SetBit(queryFriendID, eObjectBits::CHARACTER);

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
			auto maxBestFriends = Game::playerContainer.GetMaxNumberOfBestFriends();
			if (requestee.countOfBestFriends >= maxBestFriends || requestor.countOfBestFriends >= maxBestFriends) {
				if (requestee.countOfBestFriends >= maxBestFriends) {
					SendFriendResponse(requestor, requestee, eAddFriendResponseType::THEIRFRIENDLISTFULL, false);
				}
				if (requestor.countOfBestFriends >= maxBestFriends) {
					SendFriendResponse(requestor, requestee, eAddFriendResponseType::YOURFRIENDSLISTFULL, false);
				}
			} else {
				// Then update the database with this new info.
				Database::Get()->SetBestFriendStatus(requestorPlayerID, requestee.playerID, bestFriendStatus);
				// Sent the best friend update here if the value is 3
				if (bestFriendStatus == 3U) {
					if (requestee.worldServerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) SendFriendResponse(requestee, requestor, eAddFriendResponseType::ACCEPTED, false, true);
					if (requestor.worldServerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) SendFriendResponse(requestor, requestee, eAddFriendResponseType::ACCEPTED, false, true);

					for (auto& friendData : requestor.friends) {
						if (friendData.friendID == requestee.playerID) {
							friendData.isBestFriend = true;
						}
					}
					requestor.countOfBestFriends += 1;

					auto& toModify = Game::playerContainer.GetPlayerDataMutable(playerName);
					if (toModify) {
						for (auto& friendData : toModify.friends) {
							if (friendData.friendID == requestor.playerID) {
								friendData.isBestFriend = true;
							}
						}
						toModify.countOfBestFriends += 1;
					}
				}
			}
		} else {
			if (requestor.worldServerSysAddr != UNASSIGNED_SYSTEM_ADDRESS) SendFriendResponse(requestor, requestee, eAddFriendResponseType::WAITINGAPPROVAL, true, true);
		}
	} else {
		auto maxFriends = Game::playerContainer.GetMaxNumberOfFriends();
		if (requestee.friends.size() >= maxFriends) {
			SendFriendResponse(requestor, requestee, eAddFriendResponseType::THEIRFRIENDLISTFULL, false);
		} else if (requestor.friends.size() >= maxFriends) {
			SendFriendResponse(requestor, requestee, eAddFriendResponseType::YOURFRIENDSLISTFULL, false);
		} else {
			// Do not send this if we are requesting to be a best friend.
			SendFriendRequest(requestee, requestor);
		}
	}
}

void ChatPacketHandler::HandleFriendResponse(const ChatPackets::AddFriendResponse& response, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = response.playerID;
	const eAddFriendResponseCode clientResponseCode = response.responseCode;
	const LUWString& friendName = response.friendName;

	//Now to try and find both of these:
	auto& requestor = Game::playerContainer.GetPlayerDataMutable(playerID);
	auto& requestee = Game::playerContainer.GetPlayerDataMutable(friendName.GetAsString());
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
		for (const auto& friendData : requestor.friends) {
			if (friendData.friendID == requestee.playerID) {
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
		auto& requestorData = requestee.friends.emplace_back();
		requestorData.zoneID = requestor.zoneID;
		requestorData.friendID = requestor.playerID;
		requestorData.friendName = requestor.playerName;
		requestorData.isBestFriend = false;
		requestorData.isFTP = false;
		requestorData.isOnline = true;

		auto& requesteeData = requestor.friends.emplace_back();
		requesteeData.zoneID = requestee.zoneID;
		requesteeData.friendID = requestee.playerID;
		requesteeData.friendName = requestee.playerName;
		requesteeData.isBestFriend = false;
		requesteeData.isFTP = false;
		requesteeData.isOnline = true;

		Database::Get()->AddFriend(requestor.playerID, requestee.playerID);
	}

	if (serverResponseCode != eAddFriendResponseType::DECLINED) SendFriendResponse(requestor, requestee, serverResponseCode, isAlreadyBestFriends);
	if (serverResponseCode != eAddFriendResponseType::ALREADYFRIEND) SendFriendResponse(requestee, requestor, serverResponseCode, isAlreadyBestFriends);
}

void ChatPacketHandler::HandleRemoveFriend(const ChatPackets::RemoveFriend& request, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = request.playerID;
	auto friendName = request.friendName.GetAsString();

	//we'll have to query the db here to find the user, since you can delete them while they're offline.
	//First, we need to find their ID:
	LWOOBJID friendID = 0;
	auto friendIdResult = Database::Get()->GetCharacterInfo(friendName);
	if (friendIdResult) {
		friendID = friendIdResult->id;
	}

	// Convert friendID to LWOOBJID
	GeneralUtils::SetBit(friendID, eObjectBits::CHARACTER);

	Database::Get()->RemoveFriend(playerID, friendID);

	//Now, we need to send an update to notify the sender (and possibly, receiver) that their friendship has been ended:
	auto& goonA = Game::playerContainer.GetPlayerDataMutable(playerID);
	if (goonA) {
		// Remove the friend from our list of friends
		for (auto friendData = goonA.friends.cbegin(); friendData != goonA.friends.cend(); friendData++) {
			if (friendData->friendID == friendID) {
				if (friendData->isBestFriend) --goonA.countOfBestFriends;
				goonA.friends.erase(friendData);
				break;
			}
		}
		SendRemoveFriend(goonA, friendName, true);
	}

	auto& goonB = Game::playerContainer.GetPlayerDataMutable(friendID);
	if (!goonB) return;

	// Do it again for other person
	for (auto friendData = goonB.friends.cbegin(); friendData != goonB.friends.cend(); friendData++) {
		if (friendData->friendID == playerID) {
			if (friendData->isBestFriend) --goonB.countOfBestFriends;
			goonB.friends.erase(friendData);
			break;
		}
	}

	std::string goonAName = GeneralUtils::UTF16ToWTF8(Game::playerContainer.GetName(playerID));
	SendRemoveFriend(goonB, goonAName, true);
}

void ChatPacketHandler::HandleGMLevelUpdate(const ChatPackets::GMLevelUpdate& update, const SystemAddress& sysAddr) {
	auto& player = Game::playerContainer.GetPlayerDataMutable(update.playerID);
	if (!player) return;
	player.gmLevel = update.gmLevel;
}


void ChatPacketHandler::HandleWho(const ChatPackets::FindPlayerRequest& request, const SystemAddress& sysAddr) {

	const auto& sender = Game::playerContainer.GetPlayerData(request.requestor);
	if (!sender) return;

	const auto& player = Game::playerContainer.GetPlayerData(request.playerName.GetAsString());
	bool online = player;

	ClientPackets::WhoResponse response;
	response.isOnline = online;
	response.zoneID = player.zoneID;
	response.playerName = request.playerName;
	SendRouted(request.requestor, sender.worldServerSysAddr, response);
}

void ChatPacketHandler::HandleShowAll(const ChatPackets::ShowAllRequest& request, const SystemAddress& sysAddr) {
	const auto& sender = Game::playerContainer.GetPlayerData(request.requestor);
	if (!sender) return;

	ClientPackets::ShowAllResponse response;
	response.playerCount = Game::playerContainer.GetPlayerCount();
	response.simCount = Game::playerContainer.GetSimCount();
	response.displayIndividualPlayers = request.displayIndividualPlayers;
	response.displayZoneData = request.displayZoneData;
	if (request.displayZoneData || request.displayIndividualPlayers) {
		for (auto& [playerID, playerData] : Game::playerContainer.GetAllPlayers()) {
			if (!playerData) continue;
			response.players.push_back({ playerData.playerName, playerData.zoneID });
		}
	}
	SendRouted(request.requestor, sender.worldServerSysAddr, response);
}

// the structure the client uses to send this packet is shared in many chat messages
// that are sent to the server. Because of this, there are large gaps of unused data in chat messages
namespace {
	// Whispers and team chat, for moderation (log_chat=0 turns chat logging off, log_private_chat=0 just these)
	void LogChat(const PlayerData& sender, const std::string& channel, const std::string& message, LWOOBJID recipientId = LWOOBJID_EMPTY, const std::string& recipientName = "") {
		if (Game::config->GetValue("log_chat") == "0" || Game::config->GetValue("log_private_chat") == "0") return;
		IChatLog::ChatMessage entry;
		entry.time = static_cast<int64_t>(std::time(nullptr));
		entry.channel = channel;
		entry.senderId = sender.playerID;
		entry.senderName = sender.playerName;
		entry.recipientId = recipientId == LWOOBJID_EMPTY ? 0 : recipientId;
		entry.recipientName = recipientName;
		entry.zoneId = sender.zoneID.GetMapID();
		entry.instanceId = sender.zoneID.GetInstanceID();
		entry.cloneId = sender.zoneID.GetCloneID();
		entry.message = message;
		try {
			if (const auto info = Database::Get()->GetCharacterInfo(sender.playerID)) entry.accountId = info->accountId;
			Database::Get()->InsertChatMessage(entry);
		} catch (const std::exception& ex) {
			LOG("Couldn't log a chat message: %s", ex.what());
		}
	}
}

void ChatPacketHandler::HandleChatMessage(const ChatPackets::GeneralChatMessage& chatMessage, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = chatMessage.playerID;

	const auto& sender = Game::playerContainer.GetPlayerData(playerID);
	if (!sender || sender.GetIsMuted()) return;

	// Messages longer than MAX_MESSAGE_LENGTH were dropped when read
	const eChatChannel channel = chatMessage.chatChannel;
	const LUWString& message = chatMessage.message;

	LOG("Got a message from (%s) via [%s]: %s", sender.playerName.c_str(), StringifiedEnum::ToString(channel).data(), message.GetAsString().c_str());

	switch (channel) {
	case eChatChannel::TEAM: {
		auto* team = TeamContainer::GetTeam(playerID);
		if (team == nullptr) return;
		LogChat(sender, "team", message.GetAsString());

		for (const auto memberId : team->memberIDs) {
			const auto& otherMember = Game::playerContainer.GetPlayerData(memberId);
			if (!otherMember) return;
			SendPrivateChatMessage(sender, otherMember, otherMember, message, eChatChannel::TEAM, eChatMessageResponseCode::SENT);
		}
		break;
	}
	default:
		LOG("Unhandled Chat channel [%s]", StringifiedEnum::ToString(channel).data());
		break;
	}
}

// the structure the client uses to send this packet is shared in many chat messages
// that are sent to the server. Because of this, there are large gaps of unused data in chat messages
void ChatPacketHandler::HandlePrivateChatMessage(const ChatPackets::PrivateChatMessage& chatMessage, const SystemAddress& sysAddr) {
	const LWOOBJID playerID = chatMessage.playerID;

	const auto& sender = Game::playerContainer.GetPlayerData(playerID);
	if (!sender || sender.GetIsMuted()) return;

	const eChatChannel channel = chatMessage.chatChannel;
	if (channel != eChatChannel::PRIVATE_CHAT) LOG("WARNING: Received Private chat with the wrong channel!");

	// Messages longer than MAX_MESSAGE_LENGTH were dropped when read
	auto receiverName = chatMessage.receiverName.GetAsString();
	const LUWString& message = chatMessage.message;

	LOG("Got a message from (%s) via [%s]: %s to %s", sender.playerName.c_str(), StringifiedEnum::ToString(channel).data(), message.GetAsString().c_str(), receiverName.c_str());

	const auto& receiver = Game::playerContainer.GetPlayerData(receiverName);
	if (!receiver) {
		PlayerData otherPlayer;
		otherPlayer.playerName = receiverName;
		auto responseType = Database::Get()->GetCharacterInfo(receiverName)
			? eChatMessageResponseCode::NOTONLINE
			: eChatMessageResponseCode::GENERALERROR;

		SendPrivateChatMessage(sender, otherPlayer, sender, message, eChatChannel::GENERAL, responseType);
		return;
	}

	// Check to see if they are friends
	// only freinds can whispr each other
	for (const auto& fr : receiver.friends) {
		if (fr.friendID == sender.playerID) {
			LogChat(sender, "whisper", message.GetAsString(), receiver.playerID, receiver.playerName);
			//To the sender:
			SendPrivateChatMessage(sender, receiver, sender, message, eChatChannel::PRIVATE_CHAT, eChatMessageResponseCode::SENT);
			//To the receiver:
			SendPrivateChatMessage(sender, receiver, receiver, message, eChatChannel::PRIVATE_CHAT, eChatMessageResponseCode::RECEIVEDNEWWHISPER);
			return;
		}
	}
	SendPrivateChatMessage(sender, receiver, sender, message, eChatChannel::GENERAL, eChatMessageResponseCode::NOTFRIENDS);
}

void ChatPacketHandler::OnAchievementNotify(ChatPackets::AchievementNotify& notify, const SystemAddress& sysAddr) {
	const auto& playerData = Game::playerContainer.GetPlayerData(notify.earnerName.GetAsString());
	if (!playerData) return;

	for (const auto& myFriend : playerData.friends) {
		auto& friendData = Game::playerContainer.GetPlayerData(myFriend.friendID);
		if (friendData) {
			notify.targetPlayerName.string = GeneralUtils::ASCIIToUTF16(friendData.playerName);
			LOG_DEBUG("Sending achievement notify to %s", notify.targetPlayerName.GetAsString().c_str());

			SendRouted(friendData.playerID, friendData.worldServerSysAddr, notify);
		}
	}
}

void ChatPacketHandler::SendPrivateChatMessage(const PlayerData& sender, const PlayerData& receiver, const PlayerData& routeTo, const LUWString& message, const eChatChannel channel, const eChatMessageResponseCode responseCode) {
	ChatPackets::Client::PrivateChatMessage chatMessage;
	chatMessage.playerID = sender.playerID;
	chatMessage.chatChannel = channel;
	chatMessage.senderName = LUWString(sender.playerName);
	chatMessage.senderID = sender.playerID;
	chatMessage.senderGMLevel = sender.gmLevel;
	chatMessage.receiverName = LUWString(receiver.playerName);
	chatMessage.receiverGMLevel = receiver.gmLevel;
	chatMessage.responseCode = responseCode;
	chatMessage.message = message;
	SendRouted(routeTo.playerID, routeTo.worldServerSysAddr, chatMessage);
}

void ChatPacketHandler::SendFriendUpdate(const PlayerData& friendData, const PlayerData& playerData, uint8_t notifyType, uint8_t isBestFriend) {
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

	ClientPackets::UpdateFriendNotify notify;
	notify.notifyType = notifyType;

	std::string playerName = playerData.playerName.c_str();

	notify.friendName = LUWString(playerName);

	const auto& zoneID = playerData.zoneID;
	const LWOCLONEID cloneID = zoneID.GetCloneID() == friendData.zoneID.GetCloneID() ? 0 : zoneID.GetCloneID();
	notify.zoneID = LWOZONEID(zoneID.GetMapID(), zoneID.GetInstanceID(), cloneID);

	notify.isBestFriend = isBestFriend; //isBFF
	notify.isFreeTrial = 0; //isFTP

	SendRouted(friendData.playerID, friendData.worldServerSysAddr, notify);
}

void ChatPacketHandler::SendFriendRequest(const PlayerData& receiver, const PlayerData& sender) {
	//Make sure people aren't requesting people that they're already friends with:
	for (const auto& fr : receiver.friends) {
		if (fr.friendID == sender.playerID) {
			SendFriendResponse(sender, receiver, eAddFriendResponseType::ALREADYFRIEND, fr.isBestFriend);
			return; //we have this player as a friend, yeet this function so it doesn't send another request.
		}
	}

	ClientPackets::AddFriendRequest request;
	request.requestorName = LUWString(sender.playerName);
	request.isBestFriendRequest = 0; // This is a BFF flag however this is unused in live and does not have an implementation client side.
	SendRouted(receiver.playerID, receiver.worldServerSysAddr, request);
}

void ChatPacketHandler::SendFriendResponse(const PlayerData& receiver, const PlayerData& sender, eAddFriendResponseType responseCode, uint8_t isBestFriendsAlready, uint8_t isBestFriendRequest) {
	ClientPackets::AddFriendResponse response;
	response.responseCode = responseCode;
	// For all requests besides accepted, write a flag that says whether or not we are already best friends with the receiver.
	response.isOnlineOrBestFriend = responseCode != eAddFriendResponseType::ACCEPTED ? isBestFriendsAlready : sender.worldServerSysAddr != UNASSIGNED_SYSTEM_ADDRESS;
	// Then write the player name
	response.friendName = LUWString(sender.playerName);
	// Then if this is an acceptance code, write the following extra info.
	response.friendID = sender.playerID;
	response.zoneID = sender.zoneID;
	response.isBestFriend = isBestFriendRequest; //isBFF
	response.isFreeTrial = 0; //isFTP
	SendRouted(receiver.playerID, receiver.worldServerSysAddr, response);
}

void ChatPacketHandler::SendRemoveFriend(const PlayerData& receiver, std::string& personToRemove, bool isSuccessful) {
	ClientPackets::RemoveFriendResponse response;
	response.isSuccessful = isSuccessful; //isOnline
	response.friendName = LUWString(personToRemove);
	SendRouted(receiver.playerID, receiver.worldServerSysAddr, response);
}
