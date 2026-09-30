/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "ChatPackets.h"
#include "RakNetTypes.h"
#include "BitStream.h"
#include "Game.h"
#include "BitStreamUtils.h"
#include "dServer.h"
#include "ServiceType.h"
#include "MessageType/Chat.h"

namespace {
	// A zone ID as three fields: map (u16), instance (u16), clone (u32)
	void WriteZone(RakNet::BitStream& bitStream, const LWOZONEID& zoneID) {
		bitStream.Write(zoneID.GetMapID());
		bitStream.Write(zoneID.GetInstanceID());
		bitStream.Write(zoneID.GetCloneID());
	}

	bool ReadZone(RakNet::BitStream& bitStream, LWOZONEID& zoneID) {
		LWOMAPID mapID{};
		LWOINSTANCEID instanceID{};
		LWOCLONEID cloneID{};
		VALIDATE_READ(bitStream.Read(mapID));
		VALIDATE_READ(bitStream.Read(instanceID));
		VALIDATE_READ(bitStream.Read(cloneID));
		zoneID = LWOZONEID(mapID, instanceID, cloneID);
		return true;
	}

	// The LUWString fills the rest of the stream (a message whose length isn't in the packet)
	bool ReadRemainingWString(RakNet::BitStream& bitStream, LUWString& value) {
		value.size = static_cast<uint32_t>(BITS_TO_BYTES(bitStream.GetNumberOfUnreadBits()) / sizeof(char16_t));
		return value.size == 0 || bitStream.Read(value);
	}

	// Player and team packets from the client all start with the player's object ID and 4 unused bytes
	bool ReadPlayerHeader(RakNet::BitStream& bitStream, LWOOBJID& playerID, uint32_t& unknown) {
		VALIDATE_READ(bitStream.Read(playerID));
		VALIDATE_READ(bitStream.Read(unknown));
		return true;
	}
}

namespace ChatPackets {
	void LoginSessionNotify::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, playerName);
		WriteZone(bitStream, zoneID);
		bitStream.Write(muteExpire);
		bitStream.Write(gmLevel);
		// Only on a resync, so every other one is byte for byte what it always was
		if (resync) bitStream.Write<uint8_t>(1);
	}

	bool LoginSessionNotify::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, playerName, MAX_NAME_LENGTH));
		VALIDATE_READ(ReadZone(bitStream, zoneID));
		VALIDATE_READ(bitStream.Read(muteExpire));
		VALIDATE_READ(bitStream.Read(gmLevel));
		uint8_t resyncValue{};
		resync = bitStream.GetNumberOfUnreadBits() >= 8 && bitStream.Read(resyncValue) && resyncValue != 0;
		return true;
	}

	void UnexpectedDisconnect::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool UnexpectedDisconnect::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void GMLevelUpdate::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(gmLevel);
	}

	bool GMLevelUpdate::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		VALIDATE_READ(bitStream.Read(gmLevel));
		return true;
	}

	void GMMute::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(expire);
	}

	bool GMMute::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		VALIDATE_READ(bitStream.Read(expire));
		return true;
	}

	void Announcement::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, title);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, message);
	}

	bool Announcement::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, title));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, message));
		return true;
	}

	void CreateTeam::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(leaderID);
		bitStream.Write<uint64_t>(members.size());
		for (const auto member : members) bitStream.Write(member);
		WriteZone(bitStream, zoneID);
	}

	bool CreateTeam::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(leaderID));
		uint64_t count{};
		VALIDATE_READ(bitStream.Read(count));
		if (count > MAX_MEMBERS) return false;
		members.resize(count);
		for (auto& member : members) VALIDATE_READ(bitStream.Read(member));
		VALIDATE_READ(ReadZone(bitStream, zoneID));
		return true;
	}

	void TeamUpdate::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(teamID);
		bitStream.Write(deleteTeam);
		if (deleteTeam) return;
		bitStream.Write(lootFlag);
		bitStream.Write<uint8_t>(members.size());
		for (const auto member : members) bitStream.Write(member);
	}

	bool TeamUpdate::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(teamID));
		VALIDATE_READ(bitStream.Read(deleteTeam));
		if (deleteTeam) return true;
		VALIDATE_READ(bitStream.Read(lootFlag));
		uint8_t count{};
		VALIDATE_READ(bitStream.Read(count));
		members.resize(count);
		for (auto& member : members) VALIDATE_READ(bitStream.Read(member));
		return true;
	}

	void AchievementNotify::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint64_t>(0); // Packing
		bitStream.Write<uint32_t>(0); // Packing
		bitStream.Write<uint8_t>(0); // Packing
		bitStream.Write(earnerName);
		bitStream.Write<uint64_t>(0); // Packing / No way to know meaning because of not enough data.
		bitStream.Write<uint32_t>(0); // Packing / No way to know meaning because of not enough data.
		bitStream.Write<uint16_t>(0); // Packing / No way to know meaning because of not enough data.
		bitStream.Write<uint8_t>(0); // Packing / No way to know meaning because of not enough data.
		bitStream.Write(missionEmailID);
		bitStream.Write(earningPlayerID);
		bitStream.Write(targetPlayerName);
	}

	bool AchievementNotify::Deserialize(RakNet::BitStream& bitStream) {
		bitStream.IgnoreBytes(13);
		VALIDATE_READ(bitStream.Read(earnerName));
		bitStream.IgnoreBytes(15);
		VALIDATE_READ(bitStream.Read(missionEmailID));
		VALIDATE_READ(bitStream.Read(earningPlayerID));
		VALIDATE_READ(bitStream.Read(targetPlayerName));
		return true;
	}

	void MailNotify::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(receiverID);
	}

	bool MailNotify::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(receiverID));
		return true;
	}

	void ShowAllRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(requestor);
		bitStream.Write(displayZoneData);
		bitStream.Write(displayIndividualPlayers);
	}

	bool ShowAllRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(requestor));
		VALIDATE_READ(bitStream.Read(displayZoneData));
		VALIDATE_READ(bitStream.Read(displayIndividualPlayers));
		return true;
	}

	void FindPlayerRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(requestor);
		bitStream.Write(playerName);
	}

	bool FindPlayerRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(requestor));
		VALIDATE_READ(bitStream.Read(playerName));
		return true;
	}

	void GetFriendsList::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool GetFriendsList::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void AddFriendRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(friendName);
		bitStream.Write(isBestFriendRequest);
	}

	bool AddFriendRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(friendName));
		VALIDATE_READ(bitStream.Read(isBestFriendRequest));
		return true;
	}

	void AddFriendResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(responseCode);
		bitStream.Write(friendName);
	}

	bool AddFriendResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(responseCode));
		VALIDATE_READ(bitStream.Read(friendName));
		return true;
	}

	void RemoveFriend::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(friendName);
	}

	bool RemoveFriend::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(friendName));
		return true;
	}

	void GetIgnoreList::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool GetIgnoreList::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void AddIgnore::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(playerName);
	}

	bool AddIgnore::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(playerName));
		return true;
	}

	void RemoveIgnore::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(playerName);
	}

	bool RemoveIgnore::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(playerName));
		return true;
	}

	void GeneralChatMessage::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(chatChannel);
		bitStream.Write(messageLength);
		bitStream.Write(senderName);
		bitStream.Write(senderID);
		bitStream.Write(sourceID);
		bitStream.Write(senderGMLevel);
		bitStream.Write(message);
	}

	bool GeneralChatMessage::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(chatChannel));
		VALIDATE_READ(bitStream.Read(messageLength));
		if (messageLength > MAX_MESSAGE_LENGTH) return false;
		VALIDATE_READ(bitStream.Read(senderName));
		VALIDATE_READ(bitStream.Read(senderID));
		VALIDATE_READ(bitStream.Read(sourceID));
		VALIDATE_READ(bitStream.Read(senderGMLevel));
		message = LUWString(messageLength);
		if (messageLength != 0) VALIDATE_READ(bitStream.Read(message));
		return true;
	}

	void PrivateChatMessage::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(chatChannel);
		bitStream.Write(messageLength);
		bitStream.Write(senderName);
		bitStream.Write(senderID);
		bitStream.Write(sourceID);
		bitStream.Write(senderGMLevel);
		bitStream.Write(receiverName);
		bitStream.Write(receiverGMLevel);
		bitStream.Write(responseCode);
		bitStream.Write(message);
	}

	bool PrivateChatMessage::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(chatChannel));
		VALIDATE_READ(bitStream.Read(messageLength));
		if (messageLength > MAX_MESSAGE_LENGTH) return false;
		VALIDATE_READ(bitStream.Read(senderName));
		VALIDATE_READ(bitStream.Read(senderID));
		VALIDATE_READ(bitStream.Read(sourceID));
		VALIDATE_READ(bitStream.Read(senderGMLevel));
		VALIDATE_READ(bitStream.Read(receiverName));
		VALIDATE_READ(bitStream.Read(receiverGMLevel));
		VALIDATE_READ(bitStream.Read(responseCode));
		message = LUWString(messageLength);
		if (messageLength != 0) VALIDATE_READ(bitStream.Read(message));
		return true;
	}

	void TeamInvite::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(invitedPlayer);
	}

	bool TeamInvite::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(invitedPlayer));
		return true;
	}

	void TeamInviteResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(declined);
		bitStream.Write(leaderID);
	}

	bool TeamInviteResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(declined));
		VALIDATE_READ(bitStream.Read(leaderID));
		return true;
	}

	void TeamLeave::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
	}

	bool TeamLeave::Deserialize(RakNet::BitStream& bitStream) {
		return ReadPlayerHeader(bitStream, playerID, unknown);
	}

	void TeamKick::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(kickedPlayer);
	}

	bool TeamKick::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(kickedPlayer));
		return true;
	}

	void TeamSetLeader::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(promotedPlayer);
	}

	bool TeamSetLeader::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(promotedPlayer));
		return true;
	}

	void TeamSetLoot::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(lootFlag);
	}

	bool TeamSetLoot::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(lootFlag));
		return true;
	}

	void TeamGetStatus::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool TeamGetStatus::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void RequestMinimumChatMode::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(chatChannel);
	}

	bool RequestMinimumChatMode::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(chatChannel));
		return true;
	}

	void RequestMinimumChatModePrivate::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(unknown);
		bitStream.Write(chatChannel);
		bitStream.Write(recipientName);
	}

	bool RequestMinimumChatModePrivate::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPlayerHeader(bitStream, playerID, unknown));
		VALIDATE_READ(bitStream.Read(chatChannel));
		VALIDATE_READ(bitStream.Read(recipientName));
		return true;
	}
}

namespace ChatPackets::Client {
	void GeneralChatMessage::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(unknown);
		bitStream.Write(chatChannel);
		bitStream.Write<uint32_t>(message.size());
		bitStream.Write(senderName);
		bitStream.Write(senderID);
		bitStream.Write(sourceID);
		bitStream.Write(senderGMLevel);
		bitStream.Write(message);
		bitStream.Write<uint16_t>(0); // terminator
	}

	bool GeneralChatMessage::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(unknown));
		VALIDATE_READ(bitStream.Read(chatChannel));
		uint32_t length{};
		VALIDATE_READ(bitStream.Read(length));
		if (length > MAX_MESSAGE_LENGTH) return false;
		VALIDATE_READ(bitStream.Read(senderName));
		VALIDATE_READ(bitStream.Read(senderID));
		VALIDATE_READ(bitStream.Read(sourceID));
		VALIDATE_READ(bitStream.Read(senderGMLevel));
		message.resize(length);
		if (length != 0) VALIDATE_READ(bitStream.ReadBits(reinterpret_cast<unsigned char*>(message.data()), BYTES_TO_BITS(length * sizeof(char16_t)), true));
		uint16_t terminator{};
		VALIDATE_READ(bitStream.Read(terminator));
		return true;
	}

	void PrivateChatMessage::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(chatChannel);
		bitStream.Write(messageLength);
		bitStream.Write(senderName);
		bitStream.Write(senderID);
		bitStream.Write(sourceID);
		bitStream.Write(senderGMLevel);
		bitStream.Write(receiverName);
		bitStream.Write(receiverGMLevel);
		bitStream.Write(responseCode);
		bitStream.Write(message);
	}

	bool PrivateChatMessage::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		VALIDATE_READ(bitStream.Read(chatChannel));
		VALIDATE_READ(bitStream.Read(messageLength));
		VALIDATE_READ(bitStream.Read(senderName));
		VALIDATE_READ(bitStream.Read(senderID));
		VALIDATE_READ(bitStream.Read(sourceID));
		VALIDATE_READ(bitStream.Read(senderGMLevel));
		VALIDATE_READ(bitStream.Read(receiverName));
		VALIDATE_READ(bitStream.Read(receiverGMLevel));
		VALIDATE_READ(bitStream.Read(responseCode));
		VALIDATE_READ(ReadRemainingWString(bitStream, message));
		return true;
	}
}

void ChatPackets::SendSystemMessage(const SystemAddress& sysAddr, const std::string& message, const bool broadcast) {
	ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(message), broadcast);
}

void ChatPackets::SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, const bool broadcast) {
	Client::GeneralChatMessage chatMessage;
	chatMessage.chatChannel = 4;
	chatMessage.senderName = LUWString("", 33);
	chatMessage.message = message;

	RakNet::BitStream bitStream;
	chatMessage.WritePacket(bitStream);
	// A message with an address goes only to that client (so Wincent's announcement works)
	Game::server->Send(bitStream, sysAddr, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);
}

void ChatPackets::RoutedFromClient::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(senderID);
	for (const auto byte : data) bitStream.Write(byte);
}

bool ChatPackets::RoutedFromClient::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(senderID));
	data.resize(BITS_TO_BYTES(bitStream.GetNumberOfUnreadBits()));
	if (!data.empty()) VALIDATE_READ(bitStream.ReadBits(data.data(), BYTES_TO_BITS(data.size()), true));
	return true;
}

// ---- Guilds ----

void ChatPackets::GuildInvite::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(playerID);
	bitStream.Write(unknown);
	bitStream.Write(invitedPlayer);
}

bool ChatPackets::GuildInvite::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(playerID));
	VALIDATE_READ(bitStream.Read(unknown));
	VALIDATE_READ(bitStream.Read(invitedPlayer));
	return true;
}

void ChatPackets::GuildInviteResponse::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(playerID);
	bitStream.Write(unknown);
	bitStream.Write(declined);
}

bool ChatPackets::GuildInviteResponse::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(playerID));
	VALIDATE_READ(bitStream.Read(unknown));
	VALIDATE_READ(bitStream.Read(declined));
	return true;
}

void ChatPackets::GuildLeave::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(playerID);
	bitStream.Write(unknown);
}

bool ChatPackets::GuildLeave::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(playerID));
	VALIDATE_READ(bitStream.Read(unknown));
	return true;
}

void ChatPackets::GuildGetAll::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(playerID);
	bitStream.Write(unknown);
}

bool ChatPackets::GuildGetAll::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(playerID));
	VALIDATE_READ(bitStream.Read(unknown));
	return true;
}

void ChatPackets::GuildCreate::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(playerID);
	bitStream.Write(guildName);
}

bool ChatPackets::GuildCreate::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(playerID));
	VALIDATE_READ(bitStream.Read(guildName));
	return true;
}

void ChatPackets::GuildKick::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(playerID);
	bitStream.Write(unknown);
	bitStream.Write(kickedPlayer);
}

bool ChatPackets::GuildKick::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(playerID));
	VALIDATE_READ(bitStream.Read(unknown));
	VALIDATE_READ(bitStream.Read(kickedPlayer));
	return true;
}

void ChatPackets::GuildSetRank::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(playerID);
	bitStream.Write(targetPlayer);
	bitStream.Write(rank);
}

bool ChatPackets::GuildSetRank::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(playerID));
	VALIDATE_READ(bitStream.Read(targetPlayer));
	VALIDATE_READ(bitStream.Read(rank));
	return true;
}

void ChatPackets::GuildDisband::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(playerID);
}

bool ChatPackets::GuildDisband::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(playerID));
	return true;
}

void ChatPackets::GuildStatus::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(characterID);
	bitStream.Write(guildID);
	bitStream.Write(guildName);
}

bool ChatPackets::GuildStatus::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(characterID));
	VALIDATE_READ(bitStream.Read(guildID));
	VALIDATE_READ(bitStream.Read(guildName));
	return true;
}

void ChatPackets::MatchRequest::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(playerID);
	bitStream.Write(type);
	bitStream.Write(value);
	bitStream.Write(activityID);
	BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, playerName);
	BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, playerChoices);
	bitStream.Write(instanceMapID);
	bitStream.Write(minTeams);
	bitStream.Write(maxTeams);
	bitStream.Write(minTeamSize);
	bitStream.Write(maxTeamSize);
	bitStream.Write(waitTime);
	bitStream.Write(startDelay);
}

bool ChatPackets::MatchRequest::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(playerID));
	VALIDATE_READ(bitStream.Read(type));
	if (type != eMatchRequestType::JOIN && type != eMatchRequestType::READY && type != eMatchRequestType::LEAVE) return false;
	VALIDATE_READ(bitStream.Read(value));
	VALIDATE_READ(bitStream.Read(activityID));
	VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, playerName, LoginSessionNotify::MAX_NAME_LENGTH));
	VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, playerChoices, 4096));
	VALIDATE_READ(bitStream.Read(instanceMapID));
	VALIDATE_READ(bitStream.Read(minTeams));
	VALIDATE_READ(bitStream.Read(maxTeams));
	VALIDATE_READ(bitStream.Read(minTeamSize));
	VALIDATE_READ(bitStream.Read(maxTeamSize));
	VALIDATE_READ(bitStream.Read(waitTime));
	VALIDATE_READ(bitStream.Read(startDelay));
	return true;
}

void ChatPackets::MatchTransfer::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(activityID);
	WriteZone(bitStream, zoneID);
	BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, serverIP);
	bitStream.Write(serverPort);
	bitStream.Write<uint8_t>(mythranShift);
	bitStream.Write<uint32_t>(players.size());
	for (const auto player : players) bitStream.Write(player);
}

bool ChatPackets::MatchTransfer::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(activityID));
	VALIDATE_READ(ReadZone(bitStream, zoneID));
	VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, serverIP, 255));
	VALIDATE_READ(bitStream.Read(serverPort));
	uint8_t shift{};
	VALIDATE_READ(bitStream.Read(shift));
	mythranShift = shift != 0;
	uint32_t count{};
	VALIDATE_READ(bitStream.Read(count));
	if (count > MAX_PLAYERS) return false;
	players.resize(count);
	for (auto& player : players) VALIDATE_READ(bitStream.Read(player));
	return true;
}
