/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "ChatPackets.h"
#include "RakNetTypes.h"
#include "BitStream.h"
#include "Game.h"
#include "PacketUtils.h"
#include "dMessageIdentifiers.h"
#include "dServer.h"

void ChatPackets::SendChatMessage(const SystemAddress& sysAddr, char chatChannel, const std::string& senderName, LWOOBJID playerObjectID, bool senderMythran, const std::u16string& message) {
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT, MSG_CHAT_GENERAL_CHAT_MESSAGE);

	bitStream.Write(static_cast<uint64_t>(0));
	bitStream.Write(chatChannel);

	bitStream.Write(static_cast<uint32_t>(message.size()));
	PacketUtils::WriteWString(bitStream, senderName, 33);

	bitStream.Write(playerObjectID);
	bitStream.Write(static_cast<uint16_t>(0));
	bitStream.Write(static_cast<char>(0));

	for (uint32_t i = 0; i < message.size(); ++i) {
		bitStream.Write(static_cast<uint16_t>(message[i]));
	}
	bitStream.Write(static_cast<uint16_t>(0));

	SEND_PACKET_BROADCAST;
}

void ChatPackets::SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, const bool broadcast) {
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, CHAT, MSG_CHAT_GENERAL_CHAT_MESSAGE);

	bitStream.Write(static_cast<uint64_t>(0));
	bitStream.Write(static_cast<char>(4));

	bitStream.Write(static_cast<uint32_t>(message.size()));
	PacketUtils::WriteWString(bitStream, "", 33);

	bitStream.Write(static_cast<uint64_t>(0));
	bitStream.Write(static_cast<uint16_t>(0));
	bitStream.Write(static_cast<char>(0));

	for (uint32_t i = 0; i < message.size(); ++i) {
		bitStream.Write(static_cast<uint16_t>(message[i]));
	}

	bitStream.Write(static_cast<uint16_t>(0));

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
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_SEND_CANNED_TEXT);
	bitStream.Write<uint8_t>(0); //response type, options above ^
	//docs say there's a wstring here-- no idea what it's for, or if it's even needed so leaving it as is for now.
	SEND_PACKET;
}
