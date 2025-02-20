/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CHATPACKETS_H
#define CHATPACKETS_H

struct SystemAddress;

#include <string>
#include "dCommonVars.h"
#include "BitStreamUtils.h"

enum class eCannedText : uint8_t {
	CHAT_DISABLED = 0,
	F2P_CHAT_DISABLED = 1
};

namespace ChatPackets {
	struct ShowAllRequest : public LUBitStream {
		LWOOBJID requestor = LWOOBJID_EMPTY;
		bool displayZoneData = true;
		bool displayIndividualPlayers = true;

		ShowAllRequest() : LUBitStream(eConnectionType::CHAT, MessageType::Chat::WHO) {};

		virtual void Serialize(RakNet::BitStream& bitStream) const override;
		virtual bool Deserialize(RakNet::BitStream& inStream) override;
	};

	struct FindPlayerRequest : public LUBitStream {
		LWOOBJID requestor = LWOOBJID_EMPTY;
		LUWString playerName;
		FindPlayerRequest() : LUBitStream(eConnectionType::CHAT, MessageType::Chat::WHO) {};

		virtual void Serialize(RakNet::BitStream& bitStream) const override;
		virtual bool Deserialize(RakNet::BitStream& inStream) override;
	};

	struct Announcement : public LUBitStream {
		std::string title;
		std::string message;

		Announcement() : LUBitStream(eConnectionType::CHAT, MessageType::Chat::GM_ANNOUNCE) {};
		virtual void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct ChatMessage : public LUBitStream {
		char chatChannel;
		std::string senderName;
		LWOOBJID playerObjectID;
		bool senderMythran;
		eChatMessageResponseCode responseCode = eChatMessageResponseCode::SENT;
		LUWString message;
		virtual void Serialize(RakNet::BitStream& bitStream) const override;
		virtual bool Deserialize(RakNet::BitStream& inStream) override;
	};

	// Should be in client packets since it is a client connection type, but whatever
	struct MessageFailure : public LUBitStream {
		eCannedText cannedText = eCannedText::CHAT_DISABLED;
	
		MessageFailure() : LUBitStream(eConnectionType::CLIENT, MessageType::Chat::SEND_CANNED_TEXT) {};
		virtual void Serialize(RakNet::BitStream& bitStream) const override;
	};
};

#endif // CHATPACKETS_H
