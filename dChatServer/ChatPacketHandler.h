#pragma once
#include "dCommonVars.h"
#include "dNetCommon.h"
#include "BitStream.h"

struct PlayerData;

enum class eAddFriendResponseType : uint8_t;

enum class eChatChannel : uint8_t {
	SYSTEMNOTIFY = 0,
	SYSTEMWARNING,
	SYSTEMERROR,
	BROADCAST,
	LOCAL,
	LOCALNOANIM,
	EMOTE,
	PRIVATE_CHAT,
	TEAM,
	TEAMLOCAL,
	GUILD,
	GUILDNOTIFY,
	PROPERTY,
	ADMIN,
	COMBATDAMAGE,
	COMBATHEALING,
	COMBATLOOT,
	COMBATEXP,
	COMBATDEATH,
	GENERAL,
	TRADE,
	LFG,
	USER
};


enum class eChatMessageResponseCode : uint8_t {
	SENT = 0,
	NOTONLINE,
	GENERALERROR,
	RECEIVEDNEWWHISPER,
	NOTFRIENDS,
	SENDERFREETRIAL,
	RECEIVERFREETRIAL,
};

namespace ChatPacketHandler {
	void HandleFriendlistRequest(Packet* packet);
	void HandleFriendRequest(Packet* packet);
	void HandleFriendResponse(Packet* packet);
	void HandleRemoveFriend(Packet* packet);
	void HandleGMLevelUpdate(Packet* packet);
	void HandleWho(Packet* packet);
	void HandleShowAll(Packet* packet);
	void HandleChatMessage(Packet* packet);
	void HandlePrivateChatMessage(Packet* packet);

	void OnAchievementNotify(RakNet::BitStream& bitstream, const SystemAddress& sysAddr);

	//FriendData is the player we're SENDING this stuff to. Player is the friend that changed state.
	void SendFriendUpdate(const PlayerData& friendData, const PlayerData& playerData, uint8_t notifyType, uint8_t isBestFriend);
	void SendPrivateChatMessage(const PlayerData& sender, const PlayerData& receiver, const PlayerData& routeTo, const LUWString& message, const eChatChannel channel, const eChatMessageResponseCode responseCode);
	void SendFriendRequest(const PlayerData& receiver, const PlayerData& sender);
	void SendFriendResponse(const PlayerData& receiver, const PlayerData& sender, eAddFriendResponseType responseCode, uint8_t isBestFriendsAlready = 0U, uint8_t isBestFriendRequest = 0U);
	void SendRemoveFriend(const PlayerData& receiver, std::string& personToRemove, bool isSuccessful);
};
