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
#include "MessageType/Chat.h"

struct ShowAllRequest{
	LWOOBJID requestor = LWOOBJID_EMPTY;
	bool displayZoneData = true;
	bool displayIndividualPlayers = true;
	void Serialize(RakNet::BitStream& bitStream);
	void Deserialize(RakNet::BitStream& inStream);
};

struct FindPlayerRequest{
	LWOOBJID requestor = LWOOBJID_EMPTY;
	LUWString playerName;
	void Serialize(RakNet::BitStream& bitStream);
	void Deserialize(RakNet::BitStream& inStream);
};

namespace ChatPackets {

	struct Announcement : public LUBitStream {
		std::string title;
		std::string message;

		Announcement() : LUBitStream(eConnectionType::CHAT, MessageType::Chat::GM_ANNOUNCE) {};
		virtual void Serialize(RakNet::BitStream& bitStream) const override;
	};

	void SendChatMessage(const SystemAddress& sysAddr, char chatChannel, const std::string& senderName, LWOOBJID playerObjectID, bool senderMythran, const std::u16string& message);
	void SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, bool broadcast = false);
	void SendMessageFail(const SystemAddress& sysAddr);
};

#endif // CHATPACKETS_H
