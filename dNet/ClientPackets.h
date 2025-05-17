/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CLIENTPACKETS_H
#define CLIENTPACKETS_H

#include <cstdint>
#include <string>
#include "BitStreamUtils.h"
#include "eGameMasterLevel.h"
#include "eChatChannel.h"
#include "dCommonVars.h"

class PositionUpdate;

struct Packet;

class User;
struct SystemAddress;
enum class eCharacterCreationResponse : uint8_t;
enum class eRenameResponse : uint8_t;

namespace ClientPackets {
	struct LoadStaticZone : public LUBitStream {
		LWOZONEID zoneID;
		uint32_t checksum = 0;
		bool editorEnabled = false;
		uint8_t editorLevel = 0;
		NiPoint3 position = NiPoint3Constant::ZERO;
		uint32_t instanceType = 0;

		LoadStaticZone() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::LOAD_STATIC_ZONE) {};
		void Serialize(RakNet::BitStream& bitStream) const override;

	};

	struct CharacterCreationResponse : public LUBitStream {
		eCharacterCreationResponse response;

		CharacterCreationResponse() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::CHARACTER_CREATE_RESPONSE) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct CharacterRenameResponse : public LUBitStream {
		eRenameResponse response;

		CharacterRenameResponse() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::CHARACTER_RENAME_RESPONSE) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct CharacterDeleteResponse : public LUBitStream {
		bool success;

		CharacterDeleteResponse() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::DELETE_CHARACTER_RESPONSE) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct TransferToWorld : public LUBitStream {
		std::string serverIP;
		uint32_t serverPort = 0;
		bool mythranShift = false;

		TransferToWorld() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::TRANSFER_TO_WORLD) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct ServerState : public LUBitStream {
		bool serverReady = false;

		ServerState() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::SERVER_STATES) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct CreateCharacter : public LUBitStream {
		LWOOBJID objid = 0;
		LOT templateID = 1;
		std::u16string name;
		eGameMasterLevel gmLevel = eGameMasterLevel::CIVILIAN;
		int32_t chatMode = 0;
		std::string xmlData;
		int64_t reputation = 0;

		CreateCharacter() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::CREATE_CHARACTER) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct ChatModerationString : public LUBitStream {
		eChatChannel chatChannel = eChatChannel::SYSTEMNOTIFY;
		uint8_t chatMode = 0;
		std::string receiver;
		std::set<std::pair<uint8_t, uint8_t>> rejectedWords;

		ChatModerationString() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::CHAT_MODERATION_STRING) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};
	
	struct GMLevelChange : public LUBitStream {
		bool success = false;
		eGameMasterLevel highestLevel = eGameMasterLevel::CIVILIAN;
		eGameMasterLevel prevLevel = eGameMasterLevel::CIVILIAN;
		eGameMasterLevel newLevel = eGameMasterLevel::CIVILIAN;

		GMLevelChange() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::MAKE_GM_RESPONSE) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct DebugOutput : public LUBitStream {
		std::string data;

		DebugOutput() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::DEBUG_OUTPUT) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct HTTPMonitorInfoResponse : public LUBitStream {
		uint16_t port = 80;
		bool openWeb = false;
		bool supportsSum = false;
		bool supportsDetail = false;
		bool supportsWho = false;
		bool supportsObjects = false;

		HTTPMonitorInfoResponse() : LUBitStream(eConnectionType::CLIENT, MessageType::Client::HTTP_MONITOR_INFO_RESPONSE) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};
};

#endif // CLIENTPACKETS_H
