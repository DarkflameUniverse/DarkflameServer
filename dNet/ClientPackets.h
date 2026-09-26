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

enum class eLoginResponse : uint8_t;

class PositionUpdate;

struct Packet;

enum class eStamps : uint32_t {
	PASSPORT_AUTH_START,
	PASSPORT_AUTH_BYPASS,
	PASSPORT_AUTH_ERROR,
	PASSPORT_AUTH_DB_SELECT_START,
	PASSPORT_AUTH_DB_SELECT_FINISH,
	PASSPORT_AUTH_DB_INSERT_START,
	PASSPORT_AUTH_DB_INSERT_FINISH,
	PASSPORT_AUTH_LEGOINT_COMMUNICATION_START,
	PASSPORT_AUTH_LEGOINT_RECEIVED,
	PASSPORT_AUTH_LEGOINT_THREAD_SPAWN,
	PASSPORT_AUTH_LEGOINT_WEBSERVICE_START,
	PASSPORT_AUTH_LEGOINT_WEBSERVICE_FINISH,
	PASSPORT_AUTH_LEGOINT_LEGOCLUB_START,
	PASSPORT_AUTH_LEGOINT_LEGOCLUB_FINISH,
	PASSPORT_AUTH_LEGOINT_THREAD_FINISH,
	PASSPORT_AUTH_LEGOINT_REPLY,
	PASSPORT_AUTH_LEGOINT_ERROR,
	PASSPORT_AUTH_LEGOINT_COMMUNICATION_END,
	PASSPORT_AUTH_LEGOINT_DISCONNECT,
	PASSPORT_AUTH_WORLD_COMMUNICATION_START,
	PASSPORT_AUTH_CLIENT_OS,
	PASSPORT_AUTH_WORLD_PACKET_RECEIVED,
	PASSPORT_AUTH_IM_COMMUNICATION_START,
	PASSPORT_AUTH_IM_LOGIN_START,
	PASSPORT_AUTH_IM_LOGIN_ALREADY_LOGGED_IN,
	PASSPORT_AUTH_IM_OTHER_LOGIN_REMOVED,
	PASSPORT_AUTH_IM_LOGIN_QUEUED,
	PASSPORT_AUTH_IM_LOGIN_RESPONSE,
	PASSPORT_AUTH_IM_COMMUNICATION_END,
	PASSPORT_AUTH_WORLD_SESSION_CONFIRM_TO_AUTH,
	PASSPORT_AUTH_WORLD_COMMUNICATION_FINISH,
	PASSPORT_AUTH_WORLD_DISCONNECT,
	NO_LEGO_INTERFACE,
	DB_ERROR,
	GM_REQUIRED,
	NO_LEGO_WEBSERVICE_XML,
	LEGO_WEBSERVICE_TIMEOUT,
	LEGO_WEBSERVICE_ERROR,
	NO_WORLD_SERVER
};

struct Stamp {
	eStamps type{};
	uint32_t value{};
	uint64_t timestamp{};

	Stamp() = default;
	Stamp(eStamps type, uint32_t value, uint64_t timestamp = time(nullptr)){
		this->type = type;
		this->value = value;
		this->timestamp = timestamp;
	}

	void Serialize(RakNet::BitStream& outBitStream) const;
	bool Deserialize(RakNet::BitStream& inBitStream);
};


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
		// Written after a u32 holding their size in bytes plus 4
		std::vector<Stamp> stamps{};

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
