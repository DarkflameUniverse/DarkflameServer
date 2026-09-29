#ifndef WORLDPACKETS_H
#define WORLDPACKETS_H

#include "BitStreamUtils.h"
#include "ChatPackets.h"
#include "dCommonVars.h"
#include "eFunnessTypes.h"
#include "eUgcResourceType.h"
#include "MessageType/Game.h"
#include "MessageType/World.h"
#include "PositionUpdate.h"
#include "RakNetTypes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Packets of ServiceType::WORLD: what a client sends to a world server. The server's answers are ClientPackets.
// The structs only read and write bytes; the world server attaches the handlers (see WorldServer.cpp).
namespace WorldPackets {
	// Base of every world packet: the dispatcher sets sysAddr before Deserialize and Handle.
	struct WorldLUBitStream : public LUBitStream {
		SystemAddress sysAddr = UNASSIGNED_SYSTEM_ADDRESS;

		WorldLUBitStream() = default;
		template<typename T>
		WorldLUBitStream(T internalPacketID) : LUBitStream(ServiceType::WORLD, internalPacketID) {}
	};

	// Client -> server. Who the client is and the session key auth gave it.
	struct Validation : public WorldLUBitStream {
		LUWString username{ 33 };
		LUWString sessionKey{ 33 };
		LUString fdbChecksum{ 32 };
		// The client may send one more byte after the checksum (a null terminator); it is ignored.

		Validation() : WorldLUBitStream(MessageType::World::VALIDATION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. No payload.
	struct CharacterListRequest : public WorldLUBitStream {
		CharacterListRequest() : WorldLUBitStream(MessageType::World::CHARACTER_LIST_REQUEST) {}
	};

	// Client -> server. A new character from the character creator.
	struct CharacterCreateRequest : public WorldLUBitStream {
		LUWString name{ 33 };
		uint32_t firstNameIndex{};
		uint32_t middleNameIndex{};
		uint32_t lastNameIndex{};
		std::array<uint8_t, 9> unknown{}; // Not used by DLU
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
		// The client sends one more byte; it is ignored.

		CharacterCreateRequest() : WorldLUBitStream(MessageType::World::CHARACTER_CREATE_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. The character the player picked to play.
	struct CharacterLoginRequest : public WorldLUBitStream {
		LWOOBJID playerID{};

		CharacterLoginRequest() : WorldLUBitStream(MessageType::World::LOGIN_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. A game message; data holds its parameters for GameMessageHandler.
	struct GameMessage : public WorldLUBitStream {
		LWOOBJID objectID{};
		MessageType::Game messageID{};
		RakNet::BitStream data;

		GameMessage() : WorldLUBitStream(MessageType::World::GAME_MSG) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server.
	struct CharacterDeleteRequest : public WorldLUBitStream {
		LWOOBJID objectID{};

		CharacterDeleteRequest() : WorldLUBitStream(MessageType::World::CHARACTER_DELETE_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server.
	struct CharacterRenameRequest : public WorldLUBitStream {
		LWOOBJID objectID{};
		LUWString name{ 33 };

		CharacterRenameRequest() : WorldLUBitStream(MessageType::World::CHARACTER_RENAME_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. The client finished loading the zone from LoadStaticZone.
	struct LevelLoadComplete : public WorldLUBitStream {
		// The zone the client loaded; not used by DLU
		uint16_t mapID{};
		uint16_t instanceID{};
		uint32_t cloneID{};

		LevelLoadComplete() : WorldLUBitStream(MessageType::World::LEVEL_LOAD_COMPLETE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. Where the player's character is.
	struct PositionUpdate : public WorldLUBitStream {
		::PositionUpdate update{};
		// Which optional sections are present
		bool hasVelocity{};
		bool hasAngularVelocity{};
		bool hasLocalSpaceInfo{};
		bool hasLinearVelocity{}; // Inside the local space info
		bool hasRemoteInputInfo{};

		PositionUpdate() : WorldLUBitStream(MessageType::World::POSITION_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. A mail request; data holds it for Mail::HandleMail (which has its own sub-header).
	struct MailPacket : public WorldLUBitStream {
		RakNet::BitStream data;

		MailPacket() : WorldLUBitStream(MessageType::World::MAIL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. A packet for the chat server, wrapped: its size, then the chat packet's own header
	// (service, id, pad) and body.
	struct RoutePacket : public WorldLUBitStream {
		uint32_t size{};
		ServiceType routedService{};
		uint32_t routedMessageID{};
		uint8_t padding{};
		std::vector<uint8_t> routedData{}; // Everything after the routed header

		RoutePacket() : WorldLUBitStream(MessageType::World::ROUTE_PACKET) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// What DLU forwards to the chat server: the first byte of the routed id, the sender, then at most size bytes
		// of the routed data from 4 bytes in (the chat server skips 4 bytes after the sender ID it reads).
		ChatPackets::RoutedFromClient ToChat(LWOOBJID senderID) const;
	};

	// Client -> server. Asks whether what the player is typing may be sent.
	struct StringCheck : public WorldLUBitStream {
		uint8_t chatLevel{};
		uint8_t requestID{};
		std::u16string receiver = std::u16string(42, u'\0'); // Always 42 characters on the wire
		std::u16string message{}; // After a u16 character count

		StringCheck() : WorldLUBitStream(MessageType::World::STRING_CHECK) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// The receiver as DLU has always used it: each character cut to one byte, all 42 of them (the text is
		// followed by nulls and whatever else the client left in its buffer), without a leading "[GM]".
		std::string GetNarrowReceiver() const;
		// The message with each character cut to one byte.
		std::string GetNarrowMessage() const;
	};

	// Client -> server. A message in the zone's chat.
	struct GeneralChatMessage : public WorldLUBitStream {
		uint8_t chatChannel{};
		uint16_t unknown{};
		// On the wire: an i32 count that includes a null terminator, the characters, then the terminator
		std::u16string message{};

		GeneralChatMessage() : WorldLUBitStream(MessageType::World::GENERAL_CHAT_MESSAGE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. The name the player typed in the guild create box (docs/Guilds.md); the client sends it only
	// when it is not in a guild. The client's name for it: MSG_WORLD_CLIENT_TMP_GUILD_CREATE.
	struct TmpGuildCreate : public WorldLUBitStream {
		static constexpr uint32_t NAME_SIZE = 31; // characters on the wire; the client puts at most 30 and a NUL
		std::u16string guildName;

		TmpGuildCreate() : WorldLUBitStream(MessageType::World::TMP_GUILD_CREATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. The client's anti cheat noticed something.
	struct HandleFunness : public WorldLUBitStream {
		float cheatInfo{};
		eFunnessTypes cheatType{};

		HandleFunness() : WorldLUBitStream(MessageType::World::HANDLE_FUNNESS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server, when its UGCUSE3DSERVICES is off (the default): the MD5 and size of one of a blueprint's files
	// (a player's model, or a car or rocket's build), answered with ClientPackets::UgcManifestResponse.
	struct RequestUgcManifestInfo : public WorldLUBitStream {
		LWOOBJID blueprintId{};
		eUgcResourceType resourceType{};

		RequestUgcManifestInfo() : WorldLUBitStream(MessageType::World::REQUEST_UGC_MANIFEST_INFO) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. Sent by the client's resource manager (LWOResMgr2Interface::FinishResourceRequest 0x0105e5c0)
	// for every blueprint file (a player's model, or a car or rocket's build) whose request did not end with HTTP
	// status 200. Status 0: the file was not downloaded (live: 1520 of 1525 packets, all four file types, on load);
	// otherwise the HTTP status (live: 5 were 404). Live answered nothing. When the downloads keep failing, the client
	// itself logs out with NET_DISCONNECT_FAILED_DOWNLOAD_UGC (MainThread_LogoutDueToConnectionFailures 0x0102b9c0).
	struct UgcDownloadFailed : public WorldLUBitStream {
		uint32_t resType{}; // eUgcResourceType, as a u32
		LWOOBJID blueprintId{};
		uint32_t statusCode{};
		LWOOBJID charId{};

		UgcDownloadFailed() : WorldLUBitStream(MessageType::World::UGC_DOWNLOAD_FAILED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. The help screen wants its top 5 issues.
	struct UIHelpTop5 : public WorldLUBitStream {
		int32_t language{}; // 0: en_US, 1: pl_US, 2: de_DE, 3: en_GB

		UIHelpTop5() : WorldLUBitStream(MessageType::World::UI_HELP_TOP_5) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};
}

#endif // WORLDPACKETS_H
