/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CHATPACKETS_H
#define CHATPACKETS_H

struct SystemAddress;

#include <string>
#include "dCommonVars.h"
#include "MessageType/Chat.h"
#include "BitStreamUtils.h"

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

		Announcement() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GM_ANNOUNCE) {};
		virtual void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct AchievementNotify : public LUBitStream {
		LUWString targetPlayerName{};
		uint32_t missionEmailID{};
		LWOOBJID earningPlayerID{};
		LUWString earnerName{};
		AchievementNotify() : LUBitStream(ServiceType::CHAT, MessageType::Chat::ACHIEVEMENT_NOTIFY) {}
		void Serialize(RakNet::BitStream& bitstream) const override;
		bool Deserialize(RakNet::BitStream& bitstream) override;
	};

	struct TeamInviteInitialResponse : public LUBitStream {
		bool inviteFailedToSend{};
		LUWString playerName{};
		TeamInviteInitialResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::TEAM_INVITE_INITIAL_RESPONSE) {}

		void Serialize(RakNet::BitStream& bitstream) const override;
		// No Deserialize needed on our end
	};

	void SendChatMessage(const SystemAddress& sysAddr, char chatChannel, const std::string& senderName, LWOOBJID playerObjectID, bool senderMythran, const std::u16string& message);
	void SendSystemMessage(const SystemAddress& sysAddr, const std::string& message, bool broadcast = false);
	void SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, bool broadcast = false);
	void SendMessageFail(const SystemAddress& sysAddr);
	void SendRoutedMsg(const LUBitStream& msg, const LWOOBJID targetID, const SystemAddress& sysAddr);
};

#endif // CHATPACKETS_H
