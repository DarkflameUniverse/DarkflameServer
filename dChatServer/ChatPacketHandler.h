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
    Sent = 0,
    NotOnline,
    GeneralError,
    ReceivedNewWhisper,
    NotFriends,
    SenderFreeTrial,
    ReceiverFreeTrial,
};

namespace ChatPacketHandler {
	void HandleFriendlistRequest(Packet* packet);
	void HandleFriendRequest(Packet* packet);
	void HandleFriendResponse(Packet* packet);
	void HandleRemoveFriend(Packet* packet);

	void HandleChatMessage(Packet* packet);
	void HandlePrivateChatMessage(Packet* packet);

	void HandleTeamInvite(Packet* packet);
	void HandleTeamInviteResponse(Packet* packet);
	void HandleTeamLeave(Packet* packet);
	void HandleTeamKick(Packet* packet);
	void HandleTeamPromote(Packet* packet);
	void HandleTeamLootOption(Packet* packet);
	void HandleTeamStatusRequest(Packet* packet);

	void SendTeamInvite(const PlayerData& receiver, const PlayerData& sender);
	void SendTeamInviteConfirm(const PlayerData& receiver, bool bLeaderIsFreeTrial, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, uint8_t ucResponseCode, std::u16string wsLeaderName);
	void SendTeamStatus(const PlayerData& receiver, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, std::u16string wsLeaderName);
	void SendTeamSetLeader(const PlayerData& receiver, LWOOBJID i64PlayerID);
	void SendTeamAddPlayer(const PlayerData& receiver, bool bIsFreeTrial, bool bLocal, bool bNoLootOnDeath, LWOOBJID i64PlayerID, std::u16string wsPlayerName, LWOZONEID zoneID);
	void SendTeamRemovePlayer(const PlayerData& receiver, bool bDisband, bool bIsKicked, bool bIsLeaving, bool bLocal, LWOOBJID i64LeaderID, LWOOBJID i64PlayerID, std::u16string wsPlayerName);
	void SendTeamSetOffWorldFlag(const PlayerData& receiver, LWOOBJID i64PlayerID, LWOZONEID zoneID);

	//FriendData is the player we're SENDING this stuff to. Player is the friend that changed state.
	void SendFriendUpdate(const PlayerData& friendData, const PlayerData& playerData, uint8_t notifyType, uint8_t isBestFriend);

	void SendFriendRequest(const PlayerData& receiver, const PlayerData& sender);
	void SendFriendResponse(const PlayerData& receiver, const PlayerData& sender, eAddFriendResponseType responseCode, uint8_t isBestFriendsAlready = 0U, uint8_t isBestFriendRequest = 0U);
	void SendRemoveFriend(const PlayerData& receiver, std::string& personToRemove, bool isSuccessful);
};
