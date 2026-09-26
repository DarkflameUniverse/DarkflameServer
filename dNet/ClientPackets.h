/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CLIENTPACKETS_H
#define CLIENTPACKETS_H

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include "BitStreamUtils.h"
#include "MessageType/Client.h"
#include "Stamps.h"

enum class eLoginResponse : uint8_t;

class PositionUpdate;

struct Packet;

enum class Language : uint32_t {
	en_US,
	pl_US,
	de_DE,
	en_GB,
};

struct ChatMessage {
	uint8_t chatChannel = 0;
	uint16_t unknown = 0;
	std::u16string message;
};

struct ChatModerationRequest {
	uint8_t chatLevel = 0;
	uint8_t requestID = 0;
	std::string receiver;
	std::string message;
};

namespace ClientPackets {
	// Server -> client. The auth server's answer to a login (the client's clientLoginResponsePacket).
	struct LoginResponse : public LUBitStream {
		static constexpr const char* DEFAULT_CDN_TICKET = "00000000-0000-0000-0000-000000000000";

		eLoginResponse responseCode{};
		LUString events[8]{}; // Event gating
		uint16_t versionMajor{};
		uint16_t versionCurrent{};
		uint16_t versionMinor{};
		LUWString userKey{ 33 };
		LUString worldServerIP{ 33 };
		LUString chatServerIP{ 33 }; // Unused
		uint16_t worldServerPort{};
		uint16_t chatServerPort{}; // Unused
		LUString cdnKey{ 33 };
		LUString cdnTicket{ DEFAULT_CDN_TICKET, 37 };
		Language language = Language::en_US;
		LUString localization{ "US", 3 };
		bool justUpgradedFromF2P{}; // Written as one byte
		bool isFreeToPlay{}; // Written as one byte
		uint64_t freeToPlayTimeRemaining{};
		// Written as a u16 character count followed by that many UTF-16 characters
		std::string errorMessage{};
		// The login's stamps (see Stamps.h)
		Stamps stamps{};

		LoginResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::LOGIN_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	ChatMessage HandleChatMessage(Packet* packet);
	PositionUpdate HandleClientPositionUpdate(Packet* packet);
	ChatModerationRequest HandleChatModerationRequest(Packet* packet);
	int32_t SendTop5HelpIssues(Packet* packet);
};

#endif // CLIENTPACKETS_H
