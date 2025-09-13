/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CLIENTPACKETS_H
#define CLIENTPACKETS_H

#include <cstdint>
#include <string>
#include "BitStreamUtils.h"
#include "MessageType/Client.h"

class PositionUpdate;

struct Packet;

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


enum class eLoginResponse : uint8_t {
	GENERAL_FAILED = 0,
	SUCCESS,
	BANNED,
	// Unused 3
	// Unused 4
	PERMISSIONS_NOT_HIGH_ENOUGH = 5,
	INVALID_USER,
	ACCOUNT_LOCKED,
	WRONG_PASS,
	ACCOUNT_ACTIVATION_PENDING,
	ACCOUNT_DISABLED,
	GAME_TIME_EXPIRED,
	FREE_TRIAL_ENDED,
	PLAY_SCHEDULE_TIME_UP,
	ACCOUNT_NOT_ACTIVATED
};

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

enum class Language : uint32_t {
	en_US,
	pl_US,
	de_DE,
	en_GB,
};

namespace ClientPackets {

	// Structs
	struct ClientLUBitStream : public LUBitStream {
		MessageType::Client messageType = MessageType::Client::LOGIN_RESPONSE;

		ClientLUBitStream() : LUBitStream(ServiceType::CLIENT) {};
		ClientLUBitStream(MessageType::Client _messageType) : LUBitStream(ServiceType::CLIENT), messageType{_messageType} {};
		virtual void Serialize(RakNet::BitStream& bitStream) const override;
		virtual bool Deserialize(RakNet::BitStream& bitStream) override;
		virtual void Handle() override {};
	};

	struct Stamp {
		eStamps type;
		uint32_t value;
		uint64_t timestamp;

		Stamp(eStamps type, uint32_t value, uint64_t timestamp = time(nullptr)) {
			this->type = type;
			this->value = value;
			this->timestamp = timestamp;
		}

		void Serialize(RakNet::BitStream& bitStream) const;
	};

	struct LoginResponse : public ClientLUBitStream {
		eLoginResponse responseCode = eLoginResponse::GENERAL_FAILED;
		std::vector<LUString> events;
		uint16_t version_major = 0;
		uint16_t version_current = 0;
		uint16_t version_minor = 0;
		LUWString userKey;
		LUString worldServerIP;
		LUString chatServerIP = LUString(""); // unused
		uint16_t worldServerPort = 0;
		uint16_t chatServerPort = 0; // unused
		LUString cdnKey = LUString("");
		LUString cdnTicket = LUString("00000000-0000-0000-0000-000000000000", 37);
		Language language = Language::en_US;
		LUString localization = LUString("US", 3);
		bool justUpgradedFromF2P = false; // written as uint8_t
		bool isF2P = false; // written as uint8_t
		uint64_t membershipTimeLeft = 0;
		std::string errorMessage;
		std::vector<Stamp> stamps;

		LoginResponse() : ClientLUBitStream(MessageType::Client::LOGIN_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
	};


	ChatMessage HandleChatMessage(Packet* packet);
	PositionUpdate HandleClientPositionUpdate(Packet* packet);
	ChatModerationRequest HandleChatModerationRequest(Packet* packet);
	int32_t SendTop5HelpIssues(Packet* packet);
};


#endif // CLIENTPACKETS_H
