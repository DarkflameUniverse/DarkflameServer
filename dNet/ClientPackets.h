/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CLIENTPACKETS_H
#define CLIENTPACKETS_H

#include <cstdint>
#include <ctime>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "BitStreamUtils.h"
#include "MessageType/Client.h"
#include "Stamps.h"
#include "dCommonVars.h"
#include "NiPoint3.h"

enum class eCharacterCreationResponse : uint8_t;
enum class eGameMasterLevel : uint8_t;
enum class eRenameResponse : uint8_t;

enum class eLoginResponse : uint8_t;

enum class Language : uint32_t {
	en_US,
	pl_US,
	de_DE,
	en_GB,
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

	// Server -> client. Tells the client which zone to load (ClientLoadStaticZonePacket in the client).
	struct LoadStaticZone : public LUBitStream {
		uint16_t mapID{};
		uint16_t instanceID{};
		uint32_t cloneID{}; // DLU has always sent 0 here
		uint32_t mapChecksum{};
		uint8_t editorEnabled{};
		uint8_t editorLevel{};
		NiPoint3 playerPosition{};
		uint32_t instanceType{}; // Change this to eventually use 4 on activity worlds

		LoadStaticZone() : LUBitStream(ServiceType::CLIENT, MessageType::Client::LOAD_STATIC_ZONE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client. The account's characters for the character select screen.
	struct CharacterListResponse : public LUBitStream {
		struct Character {
			LWOOBJID objectID{};
			uint32_t unknown1{};
			LUWString name{ 33 };
			LUWString unapprovedName{ 33 };
			bool nameRejected{}; // Written as one byte
			bool isFreeToPlay{}; // Written as one byte
			LUString unknown2{ 10 };
			uint32_t shirtColor{};
			uint32_t shirtStyle{};
			uint32_t pantsColor{};
			uint32_t hairStyle{};
			uint32_t hairColor{};
			uint32_t leftHand{};
			uint32_t rightHand{};
			uint32_t eyebrows{};
			uint32_t eyes{};
			uint32_t mouth{};
			uint32_t unknown3{};
			uint16_t zoneID{};
			uint16_t zoneInstance{};
			uint32_t zoneClone{};
			uint64_t lastLogin{};
			std::vector<LOT> equippedItems{}; // After a u16 count
		};

		// Written as a u8 count first
		uint8_t selectedCharacterIndex{}; // TODO: Pick the most recent played index. DLU always sends 0
		std::vector<Character> characters{};

		CharacterListResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::CHARACTER_LIST_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client.
	struct CharacterCreateResponse : public LUBitStream {
		eCharacterCreationResponse response{};

		CharacterCreateResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::CHARACTER_CREATE_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client.
	struct CharacterRenameResponse : public LUBitStream {
		eRenameResponse response{};

		CharacterRenameResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::CHARACTER_RENAME_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client.
	struct DeleteCharacterResponse : public LUBitStream {
		bool success{}; // Written as one byte

		DeleteCharacterResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::DELETE_CHARACTER_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client. Sends the client to another world server.
	struct TransferToWorld : public LUBitStream {
		LUString serverIP{ 33 };
		uint16_t serverPort{};
		bool mythranShift{}; // Written as one byte

		TransferToWorld() : LUBitStream(ServiceType::CLIENT, MessageType::Client::TRANSFER_TO_WORLD) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client.
	struct ServerStates : public LUBitStream {
		uint8_t serverState = 1; // If the server is receiving this request, it probably is ready anyway.

		ServerStates() : LUBitStream(ServiceType::CLIENT, MessageType::Client::SERVER_STATES) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client. The player's own character: a compressed LDF list.
	// On the wire: u32 compressed size + 9, u8 1 (compressed), u32 uncompressed size, u32 compressed size, the zlib data.
	struct CreateCharacter : public LUBitStream {
		LWOOBJID objectID{};
		LOT templateID = 1;
		std::string xmlData{};
		std::u16string name{};
		eGameMasterLevel gmLevel{};
		int32_t chatMode{}; // DLU sends the GM level
		int64_t reputation{};
		int32_t propertyCloneID{};

		CreateCharacter() : LUBitStream(ServiceType::CLIENT, MessageType::Client::CREATE_CHARACTER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client. The answer to WorldPackets::StringCheck.
	struct ChatModerationString : public LUBitStream {
		bool requestAccepted{}; // Written as one byte
		uint16_t sourceID = 0x16; // Unknown
		uint8_t requestID{};
		uint8_t chatMode{};
		LUWString receiver{ 42 };
		// (start, length) of each part that was not accepted, in 64 two byte slots; unused slots are 0
		std::set<std::pair<uint8_t, uint8_t>> rejectedSegments{};

		ChatModerationString() : LUBitStream(ServiceType::CLIENT, MessageType::Client::CHAT_MODERATION_STRING) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client.
	struct MakeGMResponse : public LUBitStream {
		bool success{}; // Written as one byte
		eGameMasterLevel highestLevel{}; // Written as u16
		eGameMasterLevel previousLevel{}; // Written as u16
		eGameMasterLevel newLevel{}; // Written as u16

		MakeGMResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::MAKE_GM_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client. DLU does not send it.
	struct HTTPMonitorInfoResponse : public LUBitStream {
		uint16_t port = 80;
		// Each written as one byte
		bool openWeb = false;
		bool supportsSum = false;
		bool supportsDetail = false;
		bool supportsWho = false;
		bool supportsObjects = false;

		HTTPMonitorInfoResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::HTTP_MONITOR_INFO_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client. Text for the client's debug output. DLU does not send it.
	struct DebugOutput : public LUBitStream {
		std::string data{}; // After a u32 length

		DebugOutput() : LUBitStream(ServiceType::CLIENT, MessageType::Client::DEBUG_OUTPUT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};
};

#endif // CLIENTPACKETS_H
