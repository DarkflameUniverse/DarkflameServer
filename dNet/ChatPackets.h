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
#include "ClientPackets.h"

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

	struct ChatLUBitStream : public LUBitStream {
		MessageType::Chat messageType = MessageType::Chat::GENERAL_CHAT_MESSAGE;

		ChatLUBitStream() : LUBitStream(ServiceType::CHAT) {};
		ChatLUBitStream(MessageType::Chat _messageType) : LUBitStream(ServiceType::CHAT), messageType{_messageType} {};

		virtual void Serialize(RakNet::BitStream& bitStream) const override;
		virtual bool Deserialize(RakNet::BitStream& bitStream) override;
		virtual void Handle() override {};
	};

	struct Announcement : public ChatLUBitStream {
		std::string title;
		std::string message;

		Announcement() : ChatLUBitStream(MessageType::Chat::GM_ANNOUNCE) {};
		virtual void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct AchievementNotify : public ChatLUBitStream {
		LUWString targetPlayerName{};
		uint32_t missionEmailID{};
		LWOOBJID earningPlayerID{};
		LUWString earnerName{};
		AchievementNotify() : ChatLUBitStream(MessageType::Chat::ACHIEVEMENT_NOTIFY) {}
		void Serialize(RakNet::BitStream& bitstream) const override;
		bool Deserialize(RakNet::BitStream& bitstream) override;
	};

	struct TeamInviteInitialResponse : public ClientPackets::ClientLUBitStream {
		bool inviteFailedToSend{};
		LUWString playerName{};
		TeamInviteInitialResponse() : ClientPackets::ClientLUBitStream(MessageType::Client::TEAM_INVITE_INITIAL_RESPONSE) {}

		void Serialize(RakNet::BitStream& bitstream) const override;
		// No Deserialize needed on our end
	};

	void SendChatMessage(const SystemAddress& sysAddr, char chatChannel, const std::string& senderName, LWOOBJID playerObjectID, bool senderMythran, const std::u16string& message);
	void SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, bool broadcast = false);
	void SendMessageFail(const SystemAddress& sysAddr);
	void SendRoutedMsg(const LUBitStream& msg, const LWOOBJID targetID, const SystemAddress& sysAddr);
};

#endif // CHATPACKETS_H
