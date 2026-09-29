#pragma once
#include "dCommonVars.h"
#include "dNetCommon.h"
#include "BitStream.h"
#include "ChatPackets.h"
#include "ClientPackets.h"
#include "WorldRoutePacket.h"
#include "eChatChannel.h"
#include "eChatMessageResponseCode.h"

struct PlayerData;
struct LUBitStream;

enum class eAddFriendResponseType : uint8_t;

namespace ChatPacketHandler {
	// Sends msg to the world server `world`, which passes it on to the client of `target`
	void SendRouted(const LWOOBJID target, const SystemAddress& world, const LUBitStream& msg, const bool broadcast = false);

	// Reads a player's friends from the database into their data, noting which are online; nothing is sent
	void LoadFriends(PlayerData& player);

	void HandleFriendlistRequest(const ChatPackets::GetFriendsList& request, const SystemAddress& sysAddr);
	void HandleFriendRequest(const ChatPackets::AddFriendRequest& request, const SystemAddress& sysAddr);
	void HandleFriendResponse(const ChatPackets::AddFriendResponse& response, const SystemAddress& sysAddr);
	void HandleRemoveFriend(const ChatPackets::RemoveFriend& request, const SystemAddress& sysAddr);
	void HandleGMLevelUpdate(const ChatPackets::GMLevelUpdate& update, const SystemAddress& sysAddr);
	void HandleRequestMinimumChatMode(const ChatPackets::RequestMinimumChatMode& request, const SystemAddress& sysAddr);
	void HandleRequestMinimumChatModePrivate(const ChatPackets::RequestMinimumChatModePrivate& request, const SystemAddress& sysAddr);
	void HandleWho(const ChatPackets::FindPlayerRequest& request, const SystemAddress& sysAddr);
	void HandleShowAll(const ChatPackets::ShowAllRequest& request, const SystemAddress& sysAddr);
	void HandleChatMessage(const ChatPackets::GeneralChatMessage& chatMessage, const SystemAddress& sysAddr);
	void HandlePrivateChatMessage(const ChatPackets::PrivateChatMessage& chatMessage, const SystemAddress& sysAddr);
	void OnAchievementNotify(ChatPackets::AchievementNotify& notify, const SystemAddress& sysAddr);

	//FriendData is the player we're SENDING this stuff to. Player is the friend that changed state.
	void SendFriendUpdate(const PlayerData& friendData, const PlayerData& playerData, uint8_t notifyType, uint8_t isBestFriend);
	void SendPrivateChatMessage(const PlayerData& sender, const PlayerData& receiver, const PlayerData& routeTo, const LUWString& message, const eChatChannel channel, const eChatMessageResponseCode responseCode);
	void SendFriendRequest(const PlayerData& receiver, const PlayerData& sender);
	void SendFriendResponse(const PlayerData& receiver, const PlayerData& sender, eAddFriendResponseType responseCode, uint8_t isBestFriendsAlready = 0U, uint8_t isBestFriendRequest = 0U);
	void SendRemoveFriend(const PlayerData& receiver, std::string& personToRemove, bool isSuccessful);
};
