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
namespace ChatPackets {
	void ShowAllRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(this->requestor);
		bitStream.Write(this->displayZoneData);
		bitStream.Write(this->displayIndividualPlayers);
	}

	bool ShowAllRequest::Deserialize(RakNet::BitStream& inStream) {
		VALIDATE_READ(inStream.Read(this->requestor));
		VALIDATE_READ(inStream.Read(this->displayZoneData));
		VALIDATE_READ(inStream.Read(this->displayIndividualPlayers));
		return true;
	}

	void FindPlayerRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(this->requestor);
		bitStream.Write(this->playerName);
	}

	bool FindPlayerRequest::Deserialize(RakNet::BitStream& inStream) {
		VALIDATE_READ(inStream.Read(this->requestor));
		VALIDATE_READ(inStream.Read(this->playerName));
		return true;
	}

	void ChatMessage::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint64_t>(0);// senderID
		bitStream.Write(chatChannel);

		bitStream.Write<uint32_t>(message.GetAsString().size());
		bitStream.Write(LUWString(senderName));

		bitStream.Write(playerObjectID); // senderID
		bitStream.Write<uint16_t>(0); // sourceID
		bitStream.Write(responseCode);
		bitStream.Write(message);
		
	}

	bool ChatMessage::Deserialize(RakNet::BitStream& inStream) {
		//TODO: Implement this
		return false;
	}
	void ChatMessage::Handle(){
		
	}

	void WorldChatMessage::Serialize(RakNet::BitStream& bitStream) const {

	}
	bool WorldChatMessage::Deserialize(RakNet::BitStream& inStream) {
		VALIDATE_READ(inStream.Read(chatChannel));
		uint16_t padding;
		VALIDATE_READ(inStream.Read(padding));
		uint32_t messageLength;
		VALIDATE_READ(inStream.Read(messageLength));
		string message_tmp;
		for (uint32_t i = 0; i < messageLength; ++i) {
			uint16_t character;
			VALIDATE_READ(inStream.Read(character));
			message_tmp.push_back(character);
		}
		
		return true;
	}
	void WorldChatMessage::Handle() {

	}


	void PrivateChatMessage::Serialize(RakNet::BitStream& bitStream) const {

	}
	bool PrivateChatMessage::Deserialize(RakNet::BitStream& inStream) {

	}
	void PrivateChatMessage::Handle() {

	}


	void UserChatMessage::Serialize(RakNet::BitStream& bitStream) const {

	}
	bool UserChatMessage::Deserialize(RakNet::BitStream& inStream) {

	}
	void UserChatMessage::Handle() {

	}

	

	void SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, const bool broadcast) {
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

	void MessageFailure::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(this->cannedText);
	}

	void Announcement::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint32_t>(title.size());
		bitStream.Write(title);
		bitStream.Write<uint32_t>(message.size());
		bitStream.Write(message);
	}
}
