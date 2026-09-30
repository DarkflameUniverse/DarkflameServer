/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CLIENTPACKETS_H
#define CLIENTPACKETS_H

#include <array>
#include <cstdint>
#include <ctime>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "eAddFriendResponseType.h"
#include "eAddIgnoreResponse.h"
#include "eBlueprintSaveResponseType.h"
#include "eGuildCreateResponse.h"
#include "eGuildInviteResponse.h"
#include "eGuildRank.h"
#include "eMatchUpdate.h"
#include "eUgcResourceType.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "NiPoint3.h"
#include "Stamps.h"

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
		uint8_t selectedCharacterIndex{}; // The character with the latest last login
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

	// Server -> client. The answer to a BBBSaveRequest (and, with PlacementFailed and no models, to UnUseBBBModel);
	// also sent on world load to give the client the property's brick built models. Each model is the blueprint's
	// ID and its data, written as a u32 byte count followed by the bytes.
	struct BlueprintSaveResponse : public LUBitStream {
		BlueprintSaveResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::BLUEPRINT_SAVE_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		struct Model {
			LWOOBJID blueprintId{};
			std::string data{};
		};

		LWOOBJID localId{};
		eBlueprintSaveResponseType reasonCode{};
		std::vector<Model> models{}; // u32 count, then the models
	};

	// Server -> client. The answer to BBBLoadItemRequest: the model item's ID once it moved to the BBB inventory.
	struct BlueprintLoadItemResponse : public LUBitStream {
		BlueprintLoadItemResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::BLUEPRINT_LOAD_RESPONSE_ITEMID) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		uint8_t success{};
		LWOOBJID itemId{};
		LWOOBJID destItemId{};
	};

	// Server -> client. The answer to REQUEST_UGC_MANIFEST_INFO: the MD5 and size of one of a blueprint's files as the
	// client has it after downloading and inflating it (BrickModels/UserMade/<id % 1000>/<id>.<ext>.sd0 from the UGC
	// server). The client checks a file it has against it and downloads the file when it differs; with valid 0 it uses
	// a file it has as it is and downloads it only when it has none. The client ignores the answer unless it is exactly
	// this long (37 bytes after the 0x53).
	struct UgcManifestResponse : public LUBitStream {
		UgcManifestResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::UGC_MANIFEST_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID blueprintId{};
		eUgcResourceType resourceType{};
		bool valid{}; // A byte
		uint32_t fileSize{}; // Of the inflated file
		std::array<uint8_t, 16> md5{}; // Of the inflated file, as bytes
	};
};

/**
 * What the chat server sends the client (routed through the player's world in a WorldRoutePacket): friends, ignore
 * list and team responses, and the team game messages. SendCannedText comes from the world.
 */
namespace ClientPackets {
	// The answer to ChatPackets::RequestMinimumChatMode; the client hands both values to its chat UI
	// (PacketHandler_MSG_CLIENT_MINIMUM_CHAT_MODE_RESPONSE)
	struct MinimumChatModeResponse : public LUBitStream {
		uint8_t chatMode{};
		uint8_t chatChannel{};

		MinimumChatModeResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::MINIMUM_CHAT_MODE_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// The answer to ChatPackets::RequestMinimumChatModePrivate
	struct MinimumChatModeResponsePrivate : public LUBitStream {
		uint8_t chatMode{};
		uint8_t chatChannel{};
		LUWString recipientName;
		uint8_t recipientGMLevel{};

		MinimumChatModeResponsePrivate() : LUBitStream(ServiceType::CLIENT, MessageType::Client::MINIMUM_CHAT_MODE_RESPONSE_PRIVATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> client: chat is off (0: "Chat is currently disabled.", 1: "Upgrade to a full LEGO Universe
	// Membership to chat with other players.")
	struct SendCannedText : public LUBitStream {
		uint8_t responseType{};

		SendCannedText() : LUBitStream(ServiceType::CLIENT, MessageType::Client::SEND_CANNED_TEXT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct GetFriendsListResponse : public LUBitStream {
		uint8_t responseCode{};
		uint16_t packetLength{ 1 }; // the client skips it
		std::vector<FriendData> friends; // u16 count

		GetFriendsListResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GET_FRIENDS_LIST_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Someone wants to be friends
	struct AddFriendRequest : public LUBitStream {
		LUWString requestorName;
		uint8_t isBestFriendRequest{}; // unused in live, and the client does nothing with it

		AddFriendRequest() : LUBitStream(ServiceType::CLIENT, MessageType::Client::ADD_FRIEND_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct AddFriendResponse : public LUBitStream {
		eAddFriendResponseType responseCode{};
		// ACCEPTED: whether the friend is online; anything else: whether they are best friends already
		uint8_t isOnlineOrBestFriend{};
		LUWString friendName;
		// ACCEPTED only
		LWOOBJID friendID{};
		LWOZONEID zoneID{};
		uint8_t isBestFriend{};
		uint8_t isFreeTrial{};

		AddFriendResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::ADD_FRIEND_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct RemoveFriendResponse : public LUBitStream {
		uint8_t isSuccessful{};
		LUWString friendName;

		RemoveFriendResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::REMOVE_FRIEND_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// A friend logged in (1), out (0) or changed worlds (2)
	struct UpdateFriendNotify : public LUBitStream {
		uint8_t notifyType{};
		LUWString friendName;
		LWOZONEID zoneID{}; // clone 0 when it is the receiver's clone
		uint8_t isBestFriend{};
		uint8_t isFreeTrial{};

		UpdateFriendNotify() : LUBitStream(ServiceType::CLIENT, MessageType::Client::UPDATE_FRIEND_NOTIFY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Answer to /findplayer
	struct WhoResponse : public LUBitStream {
		uint8_t isOnline{};
		LWOZONEID zoneID{};
		LUWString playerName;

		WhoResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::WHO_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Answer to /showall
	struct ShowAllResponse : public LUBitStream {
		struct Player {
			std::string name; // written when displayIndividualPlayers
			LWOZONEID zoneID{}; // written when displayZoneData
		};

		uint32_t playerCount{};
		uint32_t simCount{};
		bool displayIndividualPlayers{};
		bool displayZoneData{};
		// Only written when either display flag is set; there is no count, they fill the rest of the packet
		std::vector<Player> players;

		ShowAllResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::SHOW_ALL_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct GetIgnoreListResponse : public LUBitStream {
		struct Ignored {
			LWOOBJID playerID{};
			LUWString playerName{ 36u };
		};

		uint8_t isFreeTrial{};
		uint16_t padding{}; // struct alignment
		std::vector<Ignored> ignored; // u16 count

		GetIgnoreListResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GET_IGNORE_LIST_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct AddIgnoreResponse : public LUBitStream {
		eAddIgnoreResponse responseCode{};
		LUWString playerName;
		LWOOBJID playerID{};

		AddIgnoreResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::ADD_IGNORE_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct RemoveIgnoreResponse : public LUBitStream {
		int8_t responseCode{};
		LUWString playerName;

		RemoveIgnoreResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::REMOVE_IGNORE_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Someone invites the player to their team
	struct TeamInvite : public LUBitStream {
		LUWString senderName;
		LWOOBJID senderID{};

		TeamInvite() : LUBitStream(ServiceType::CLIENT, MessageType::Client::TEAM_INVITE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Whether the player's team invite went out
	struct TeamInviteInitialResponse : public LUBitStream {
		bool inviteFailedToSend{}; // one byte
		LUWString playerName{};
		TeamInviteInitialResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::TEAM_INVITE_INITIAL_RESPONSE) {}

		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	/**
	 * Team game messages, written by the chat server: the CLIENT/GAME_MSG header, the object the message is for,
	 * the game message ID, then the message's own fields (like GameMessages::NetGameMsg, which chat doesn't link).
	 */
	struct TeamGameMsg : public LUBitStream {
		LWOOBJID target{};
		MessageType::Game msgId{};

		TeamGameMsg(MessageType::Game id) : LUBitStream(ServiceType::CLIENT, MessageType::Client::GAME_MSG), msgId{ id } {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamInviteConfirm : public TeamGameMsg {
		bool bLeaderIsFreeTrial{};
		LWOOBJID i64LeaderID{};
		LWOZONEID i64LeaderZoneID{};
		uint32_t binaryBufferLength{}; // always empty
		uint8_t ucLootFlag{};
		uint8_t ucNumOfOtherPlayers{};
		uint8_t ucResponseCode{};
		std::u16string wsLeaderName; // u32 length

		TeamInviteConfirm() : TeamGameMsg(MessageType::Game::TEAM_INVITE_CONFIRM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamGetStatusResponse : public TeamGameMsg {
		LWOOBJID i64LeaderID{};
		LWOZONEID i64LeaderZoneID{};
		uint32_t binaryBufferLength{}; // always empty
		uint8_t ucLootFlag{};
		uint8_t ucNumOfOtherPlayers{};
		std::u16string wsLeaderName; // u32 length

		TeamGetStatusResponse() : TeamGameMsg(MessageType::Game::TEAM_GET_STATUS_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamSetLeader : public TeamGameMsg {
		LWOOBJID i64PlayerID{};

		TeamSetLeader() : TeamGameMsg(MessageType::Game::TEAM_SET_LEADER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamAddPlayer : public TeamGameMsg {
		bool bIsFreeTrial{};
		bool bLocal{};
		bool bNoLootOnDeath{};
		LWOOBJID i64PlayerID{};
		std::u16string wsPlayerName; // u32 length
		LWOZONEID zoneID{}; // always written (its default flag is always set); clone 0 when it's the receiver's clone

		TeamAddPlayer() : TeamGameMsg(MessageType::Game::TEAM_ADD_PLAYER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamRemovePlayer : public TeamGameMsg {
		bool bDisband{};
		bool bIsKicked{};
		bool bIsLeaving{};
		bool bLocal{};
		LWOOBJID i64LeaderID{}; // not empty: the client makes this player the leader
		LWOOBJID i64PlayerID{};
		std::u16string wsPlayerName; // u32 length

		TeamRemovePlayer() : TeamGameMsg(MessageType::Game::TEAM_REMOVE_PLAYER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamSetOffWorldFlag : public TeamGameMsg {
		LWOOBJID i64PlayerID{};
		LWOZONEID zoneID{}; // clone 0 when it's the receiver's clone

		TeamSetOffWorldFlag() : TeamGameMsg(MessageType::Game::TEAM_SET_OFF_WORLD_FLAG) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// The activity lobby's MatchUpdate, written by the chat server's matchmaking (docs/Matchmaking.md). Byte for byte
	// GameMessages::MatchUpdate: name-value text (widened to UTF-16), then the eMatchUpdate type.
	struct MatchUpdate : public TeamGameMsg {
		std::string data; // name-value text, one byte per character here
		eMatchUpdate type{};

		MatchUpdate() : TeamGameMsg(MessageType::Game::MATCH_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	/**
	 * Guild packets (docs/Guilds.md), written by the chat server and routed through the player's world. The client
	 * copies them into packed structs: every string is a fixed-size UTF-16 buffer it reads up to the first NUL, so
	 * DLU writes at most size - 1 characters.
	 */
	namespace Guild {
		// Guild names are 31 characters on the wire (30 and a NUL); player names 33
		constexpr uint32_t NAME_SIZE = 31;
		constexpr uint32_t PLAYER_NAME_SIZE = 33;
		// A UTF-8 name as a buffer of `size` characters, cut so a NUL is always left
		LUWString FixedName(const std::string& name, uint32_t size);
	}

	// The answer to the player's TMP_GUILD_CREATE
	struct GuildCreateResponse : public LUBitStream {
		eGuildCreateResponse result{};
		LWOOBJID guildID{}; // the client doesn't read it
		std::string guildName; // 31 characters on the wire

		GuildCreateResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GUILD_CREATE_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Someone invites the player to their guild; the client asks the player with a message box
	struct GuildInvite : public LUBitStream {
		std::string inviterName; // 33 characters on the wire
		std::string guildName;   // 31

		GuildInvite() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GUILD_INVITE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// What happened to the invite the player sent
	struct GuildInviteInitialResponse : public LUBitStream {
		eGuildInviteResponse response{};
		std::string playerName; // the invited player; 33 characters on the wire

		GuildInviteInitialResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GUILD_INVITE_INITIAL_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// The invited player's answer, to the inviter
	struct GuildInviteFinalResponse : public LUBitStream {
		eGuildInviteFinalResponse response{};
		std::string playerName; // the invited player; 33 characters on the wire

		GuildInviteFinalResponse() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GUILD_INVITE_FINAL_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// To the player who accepted an invite: whether they are in the guild now (the client then asks for its data)
	struct GuildInviteConfirm : public LUBitStream {
		bool failed{}; // one byte
		std::string guildName; // 33 characters on the wire

		GuildInviteConfirm() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GUILD_INVITE_CONFIRM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Someone joined the player's guild
	struct GuildAddPlayer : public LUBitStream {
		std::string playerName; // 33 characters on the wire
		LWOOBJID playerID{};
		eGuildRank rank{};
		LWOZONEID zoneID{};
		bool online{}; // one byte

		GuildAddPlayer() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GUILD_ADD_PLAYER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Someone left or was kicked from the player's guild (the player themself: the client drops the guild)
	struct GuildRemovePlayer : public LUBitStream {
		eGuildLeaveReason reason{};
		std::string playerName; // 33 characters on the wire
		LWOOBJID playerID{};
		LWOOBJID newLeaderID{}; // 0: the leader didn't change; otherwise that member's rank becomes leader

		GuildRemovePlayer() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GUILD_REMOVE_PLAYER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// A guildmate logged in or out, or changed worlds (worldUpdateOnly: no chat line)
	struct GuildLoginLogout : public LUBitStream {
		std::string playerName; // 33 characters on the wire
		LWOOBJID playerID{};
		bool online{};          // one byte
		LWOZONEID zoneID{};
		bool worldUpdateOnly{}; // one byte

		GuildLoginLogout() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GUILD_LOGIN_LOGOUT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// The player's guild and every member (the answer to GUILD_GET_ALL)
	struct GuildData : public LUBitStream {
		struct Member {
			eGuildRank rank{};
			bool online{};       // one byte
			LWOZONEID zoneID{};  // the client reads it only for online members
			LWOOBJID playerID{};
			std::string name;    // 33 characters on the wire
		};

		uint8_t status{}; // the client only takes 0 (DLU sends 1 to a player in no guild)
		std::string guildName; // 31 characters on the wire
		std::string joinDate;  // 11 characters on the wire; the client doesn't show it
		std::string foundDate; // 11
		int32_t reputation{};  // the client doesn't use it
		int32_t unknown1{};
		int32_t unknown2{};
		uint16_t unknown3{};
		std::vector<Member> members; // u16 count; the client ignores data with none

		GuildData() : LUBitStream(ServiceType::CLIENT, MessageType::Client::GUILD_DATA) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};
}

#endif // CLIENTPACKETS_H
