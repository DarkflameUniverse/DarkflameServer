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
#include "eConnectionType.h"
#include "MessageType/Chat.h"

void ShowAllRequest::Serialize(RakNet::BitStream& bitStream) {
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::SHOW_ALL);
	bitStream.Write(this->requestor);
	bitStream.Write(this->displayZoneData);
	bitStream.Write(this->displayIndividualPlayers);
}

void ShowAllRequest::Deserialize(RakNet::BitStream& inStream) {
	inStream.Read(this->requestor);
	inStream.Read(this->displayZoneData);
	inStream.Read(this->displayIndividualPlayers);
}

void FindPlayerRequest::Serialize(RakNet::BitStream& bitStream) {
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::WHO);
	bitStream.Write(this->requestor);
	bitStream.Write(this->playerName);
}

void FindPlayerRequest::Deserialize(RakNet::BitStream& inStream) {
	inStream.Read(this->requestor);
	inStream.Read(this->playerName);
}

void ChatPackets::SendChatMessage(const SystemAddress& sysAddr, char chatChannel, const std::string& senderName, LWOOBJID playerObjectID, bool senderMythran, const std::u16string& message) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::GENERAL_CHAT_MESSAGE);

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

void ChatPackets::SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, const bool broadcast) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::GENERAL_CHAT_MESSAGE);

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

void ChatPackets::SendMessageFail(const SystemAddress& sysAddr) {
	//0x00 - "Chat is currently disabled."
	//0x01 - "Upgrade to a full LEGO Universe Membership to chat with other players."

	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, MessageType::Client::SEND_CANNED_TEXT);
	bitStream.Write<uint8_t>(0); //response type, options above ^
	//docs say there's a wstring here-- no idea what it's for, or if it's even needed so leaving it as is for now.
	SEND_PACKET;
}

namespace ChatPackets {
	void Announcement::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint32_t>(title.size());
		bitStream.Write(title);
		bitStream.Write<uint32_t>(message.size());
		bitStream.Write(message);
	}
}

void ChatPackets::AchievementNotify::Serialize(RakNet::BitStream& bitstream) const {
	bitstream.Write<uint64_t>(0); // Packing
	bitstream.Write<uint32_t>(0); // Packing
	bitstream.Write<uint8_t>(0); // Packing
	bitstream.Write(earnerName);
	bitstream.Write<uint64_t>(0); // Packing / No way to know meaning because of not enough data.
	bitstream.Write<uint32_t>(0); // Packing / No way to know meaning because of not enough data.
	bitstream.Write<uint16_t>(0); // Packing / No way to know meaning because of not enough data.
	bitstream.Write<uint8_t>(0); // Packing / No way to know meaning because of not enough data.
	bitstream.Write(missionEmailID);
	bitstream.Write(earningPlayerID);
	bitstream.Write(targetPlayerName);
}

bool ChatPackets::AchievementNotify::Deserialize(RakNet::BitStream& bitstream) {
	bitstream.IgnoreBytes(13);
	VALIDATE_READ(bitstream.Read(earnerName));
	bitstream.IgnoreBytes(15);
	VALIDATE_READ(bitstream.Read(missionEmailID));
	VALIDATE_READ(bitstream.Read(earningPlayerID));
	VALIDATE_READ(bitstream.Read(targetPlayerName));

	return true;
}

void ChatPackets::TeamInviteInitialResponse::Serialize(RakNet::BitStream& bitstream) const {
	bitstream.Write<uint8_t>(inviteFailedToSend);
	bitstream.Write(playerName);
}

void ChatPackets::SendRoutedMsg(const LUBitStream& msg, const LWOOBJID targetID, const SystemAddress& sysAddr) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET);
	bitStream.Write(targetID);

	// Now write the actual packet
	msg.WriteHeader(bitStream);
	msg.Serialize(bitStream);
	Game::server->Send(bitStream, sysAddr, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);
}
