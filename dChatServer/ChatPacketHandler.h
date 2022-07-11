#pragma once
#include "dCommonVars.h"
#include "dNetCommon.h"
#include "BitStream.h"

struct PlayerData;
enum class AddFriendResponseType : uint8_t;

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

	void SendTeamInvite(PlayerData* receiver, PlayerData* sender);
	void SendTeamInviteConfirm(PlayerData* receiver, bool bLeaderIsFreeTrial, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, uint8_t ucResponseCode, std::u16string wsLeaderName);
	void SendTeamStatus(PlayerData* receiver, LWOOBJID i64LeaderID, LWOZONEID i64LeaderZoneID, uint8_t ucLootFlag, uint8_t ucNumOfOtherPlayers, std::u16string wsLeaderName);
	void SendTeamSetLeader(PlayerData* receiver, LWOOBJID i64PlayerID);
	void SendTeamAddPlayer(PlayerData* receiver, bool bIsFreeTrial, bool bLocal, bool bNoLootOnDeath, LWOOBJID i64PlayerID, std::u16string wsPlayerName, LWOZONEID zoneID);
	void SendTeamRemovePlayer(PlayerData* receiver, bool bDisband, bool bIsKicked, bool bIsLeaving, bool bLocal, LWOOBJID i64LeaderID, LWOOBJID i64PlayerID, std::u16string wsPlayerName);
	void SendTeamSetOffWorldFlag(PlayerData* receiver, LWOOBJID i64PlayerID, LWOZONEID zoneID);

	//FriendData is the player we're SENDING this stuff to. Player is the friend that changed state.
	void SendFriendUpdate(PlayerData* friendData, PlayerData* playerData, uint8_t notifyType, uint8_t isBestFriend);

	void SendFriendRequest(PlayerData* receiver, PlayerData* sender);
	void SendFriendResponse(PlayerData* receiver, PlayerData* sender, AddFriendResponseType responseCode, uint8_t isBestFriendsAlready = 0U, uint8_t isBestFriendRequest = 0U);
	void SendRemoveFriend(PlayerData* receiver, std::string& personToRemove, bool isSuccessful);
};
