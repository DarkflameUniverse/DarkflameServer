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
#include "eChatMessageType.h"

void ChatPackets::SendChatMessage(const SystemAddress& sysAddr, char chatChannel, const std::string& senderName, LWOOBJID playerObjectID, bool senderMythran, const std::u16string& message) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::GENERAL_CHAT_MESSAGE);

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
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::GENERAL_CHAT_MESSAGE);

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
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::SEND_CANNED_TEXT);
	bitStream.Write<uint8_t>(0); //response type, options above ^
	//docs say there's a wstring here-- no idea what it's for, or if it's even needed so leaving it as is for now.
	SEND_PACKET;
}
