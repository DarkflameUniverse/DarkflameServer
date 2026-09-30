/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CHATPACKETS_H
#define CHATPACKETS_H

struct SystemAddress;

#include <string>
#include <vector>
#include "dCommonVars.h"
#include "MessageType/Chat.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "BitStreamUtils.h"
#include "eAddFriendResponseCode.h"
#include "eAddFriendResponseType.h"
#include "eAddIgnoreResponse.h"
#include "eChatChannel.h"
#include "eChatMessageResponseCode.h"
#include "eGameMasterLevel.h"

/**
 * Packets of the chat service (docs/PacketArchitecture.md).
 *
 * Top level: packets between world servers and the chat server (MessageType::Chat). The ones a client sends go to its
 * world as a ROUTE_PACKET; the world forwards them to chat with the player's object ID in front, so every such
 * struct starts with `playerID`.
 *
 * ChatPackets::Client: the chat-service packets the client receives. What the chat server sends a client in other
 * services (friends, ignore list and team responses, team game messages) is in ClientPackets.h. The chat server wraps
 * them in a WorldRoutePacket (WorldRoutePacket.h) to the world the player is in, which passes the inner packet on to
 * the client unchanged.
 */
namespace ChatPackets {
	// World -> chat: a player finished loading into a world (also sent when they change worlds)
	struct LoginSessionNotify : public LUBitStream {
		static constexpr uint32_t MAX_NAME_LENGTH = 33;

		LWOOBJID playerID{};
		std::string playerName; // u32 length, then 1 byte per character
		LWOZONEID zoneID{};
		int64_t muteExpire{}; // time_t
		eGameMasterLevel gmLevel{};
		// Sent again to a new chat server after a live update restarted chat (CHAT_SERVER_READY): the player didn't
		// log in or change zones. Written last, only when set, and read only when there.
		bool resync{};

		LoginSessionNotify() : LUBitStream(ServiceType::CHAT, MessageType::Chat::LOGIN_SESSION_NOTIFY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat: a player left (or switched characters, or a character was deleted)
	struct UnexpectedDisconnect : public LUBitStream {
		LWOOBJID playerID{};

		UnexpectedDisconnect() : LUBitStream(ServiceType::CHAT, MessageType::Chat::UNEXPECTED_DISCONNECT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat: a player's GM level changed
	struct GMLevelUpdate : public LUBitStream {
		LWOOBJID playerID{};
		eGameMasterLevel gmLevel{};

		GMLevelUpdate() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GMLEVEL_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat, and chat -> every world: a player was muted until `expire` (time_t; 1 means until reviewed)
	struct GMMute : public LUBitStream {
		LWOOBJID playerID{};
		int64_t expire{};

		GMMute() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GM_MUTE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat -> every other world: a GM's /announce
	struct Announcement : public LUBitStream {
		std::string title;   // u32 length, then 1 byte per character
		std::string message; // u32 length, then 1 byte per character

		Announcement() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GM_ANNOUNCE) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat (custom DLU message): make a team of the players starting an activity together
	struct CreateTeam : public LUBitStream {
		// More can't be a team; bigger requests are dropped
		static constexpr uint64_t MAX_MEMBERS = 3;

		LWOOBJID leaderID{};
		std::vector<LWOOBJID> members; // u64 count
		LWOZONEID zoneID{};

		CreateTeam() : LUBitStream(ServiceType::CHAT, MessageType::Chat::CREATE_TEAM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Chat -> every world: a team changed (or was deleted), so worlds can share loot and credit
	struct TeamUpdate : public LUBitStream {
		LWOOBJID teamID{};
		bool deleteTeam{}; // one bit; nothing follows when set
		uint8_t lootFlag{};
		std::vector<LWOOBJID> members; // u8 count

		TeamUpdate() : LUBitStream(ServiceType::CHAT, MessageType::Chat::TEAM_GET_STATUS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat -> the earner's online friends: a mission announcement (routed to the client as is)
	struct AchievementNotify : public LUBitStream {
		LUWString targetPlayerName{};
		uint32_t missionEmailID{};
		LWOOBJID earningPlayerID{};
		LUWString earnerName{};
		AchievementNotify() : LUBitStream(ServiceType::CHAT, MessageType::Chat::ACHIEVEMENT_NOTIFY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat -> world: receiverID has new mail. DLU's own server packet: the chat server passes it on to the world
	// the receiver is in, which sends them the new-mail notification. Characters that are offline are skipped.
	struct MailNotify : public LUBitStream {
		LWOOBJID receiverID{ LWOOBJID_EMPTY };
		MailNotify() : LUBitStream(ServiceType::CHAT, MessageType::Chat::MAIL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat: /showall
	struct ShowAllRequest : public LUBitStream {
		LWOOBJID requestor = LWOOBJID_EMPTY;
		bool displayZoneData = true;         // one bit
		bool displayIndividualPlayers = true; // one bit

		ShowAllRequest() : LUBitStream(ServiceType::CHAT, MessageType::Chat::SHOW_ALL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat: /findplayer
	struct FindPlayerRequest : public LUBitStream {
		LWOOBJID requestor = LWOOBJID_EMPTY;
		LUWString playerName;

		FindPlayerRequest() : LUBitStream(ServiceType::CHAT, MessageType::Chat::WHO) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat. A chat packet the client sent through the world server (WorldPackets::RoutePacket): its id,
	// the object ID of the player who sent it (the chat server has no other way to know), then the routed bytes.
	struct RoutedFromClient : public LUBitStream {
		LWOOBJID senderID{};
		std::vector<uint8_t> data{};

		RoutedFromClient(uint8_t routedMessageID = 0) : LUBitStream(ServiceType::CHAT, routedMessageID) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> world -> chat. Every packet below starts with the player's object ID (put there by the world).
	// `unknown` is 4 bytes of the client's packet the server doesn't use.

	// A player's friends list (sent when they load into a world)
	struct GetFriendsList : public LUBitStream {
		LWOOBJID playerID{};

		GetFriendsList() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GET_FRIENDS_LIST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct AddFriendRequest : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		LUWString friendName;
		uint8_t isBestFriendRequest{};

		AddFriendRequest() : LUBitStream(ServiceType::CHAT, MessageType::Chat::ADD_FRIEND_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// The answer of the player who was asked to be friends
	struct AddFriendResponse : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		eAddFriendResponseCode responseCode{};
		LUWString friendName;

		AddFriendResponse() : LUBitStream(ServiceType::CHAT, MessageType::Chat::ADD_FRIEND_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct RemoveFriend : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		LUWString friendName;

		RemoveFriend() : LUBitStream(ServiceType::CHAT, MessageType::Chat::REMOVE_FRIEND) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct GetIgnoreList : public LUBitStream {
		LWOOBJID playerID{};

		GetIgnoreList() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GET_IGNORE_LIST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct AddIgnore : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		LUWString playerName;

		AddIgnore() : LUBitStream(ServiceType::CHAT, MessageType::Chat::ADD_IGNORE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct RemoveIgnore : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		LUWString playerName;

		RemoveIgnore() : LUBitStream(ServiceType::CHAT, MessageType::Chat::REMOVE_IGNORE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Chat in a channel the chat server handles (team chat)
	struct GeneralChatMessage : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		eChatChannel chatChannel{};
		uint32_t messageLength{}; // characters in message; more than MAX_MESSAGE_LENGTH is refused
		// What the client fills in about itself (the server uses its own records)
		LUWString senderName;
		LWOOBJID senderID{};
		uint16_t sourceID{};
		uint8_t senderGMLevel{};
		LUWString message{ 0u }; // messageLength characters

		GeneralChatMessage() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GENERAL_CHAT_MESSAGE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// A whisper
	struct PrivateChatMessage : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		eChatChannel chatChannel{};
		uint32_t messageLength{}; // characters in message; more than MAX_MESSAGE_LENGTH is refused
		// What the client fills in about itself (the server uses its own records)
		LUWString senderName;
		LWOOBJID senderID{};
		uint16_t sourceID{};
		uint8_t senderGMLevel{};
		LUWString receiverName;
		uint8_t receiverGMLevel{};
		uint8_t responseCode{};
		LUWString message{ 0u }; // messageLength characters

		PrivateChatMessage() : LUBitStream(ServiceType::CHAT, MessageType::Chat::PRIVATE_CHAT_MESSAGE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamInvite : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		LUWString invitedPlayer;

		TeamInvite() : LUBitStream(ServiceType::CHAT, MessageType::Chat::TEAM_INVITE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamInviteResponse : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		uint8_t declined{};
		LWOOBJID leaderID{};

		TeamInviteResponse() : LUBitStream(ServiceType::CHAT, MessageType::Chat::TEAM_INVITE_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamLeave : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};

		TeamLeave() : LUBitStream(ServiceType::CHAT, MessageType::Chat::TEAM_LEAVE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamKick : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		LUWString kickedPlayer;

		TeamKick() : LUBitStream(ServiceType::CHAT, MessageType::Chat::TEAM_KICK) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Make another member the team's leader
	struct TeamSetLeader : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		LUWString promotedPlayer;

		TeamSetLeader() : LUBitStream(ServiceType::CHAT, MessageType::Chat::TEAM_SET_LEADER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct TeamSetLoot : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		uint8_t lootFlag{};

		TeamSetLoot() : LUBitStream(ServiceType::CHAT, MessageType::Chat::TEAM_SET_LOOT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// A player's team, again (sent when they load into a world)
	struct TeamGetStatus : public LUBitStream {
		LWOOBJID playerID{};

		TeamGetStatus() : LUBitStream(ServiceType::CHAT, MessageType::Chat::TEAM_GET_STATUS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// The lowest chat mode among who would read the player's team chat (chatChannel 8 team, 10 local team)
	struct RequestMinimumChatMode : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		uint8_t chatChannel{};

		RequestMinimumChatMode() : LUBitStream(ServiceType::CHAT, MessageType::Chat::REQUEST_MINIMUM_CHAT_MODE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// The same for a private message to recipientName (chatChannel 7)
	struct RequestMinimumChatModePrivate : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		uint8_t chatChannel{};
		LUWString recipientName;

		RequestMinimumChatModePrivate() : LUBitStream(ServiceType::CHAT, MessageType::Chat::REQUEST_MINIMUM_CHAT_MODE_PRIVATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Guilds (docs/Guilds.md). The client sends GUILD_INVITE, GUILD_INVITE_RESPONSE, GUILD_LEAVE and GUILD_GET_ALL
	// (routed like the ones above); the others are DLU's world <-> chat packets.

	// Invite a player (by name) to the sender's guild
	struct GuildInvite : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		LUWString invitedPlayer{ 33 };

		GuildInvite() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GUILD_INVITE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// The answer to the invite in the message box (button 1 accepts: declined 0)
	struct GuildInviteResponse : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		uint8_t declined{};

		GuildInviteResponse() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GUILD_INVITE_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Leave the guild. The client sends 66 more bytes of whatever was on its stack; they are not read.
	struct GuildLeave : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};

		GuildLeave() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GUILD_LEAVE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// The player's guild and its members (answered with ClientPackets::GuildData)
	struct GuildGetAll : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};

		GuildGetAll() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GUILD_GET_ALL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat: the name from the client's TMP_GUILD_CREATE
	struct GuildCreate : public LUBitStream {
		LWOOBJID playerID{};
		LUWString guildName{ 31 };

		GuildCreate() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GUILD_CREATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat: /gkick (the client never sends GUILD_KICK; the layout is the team kick's)
	struct GuildKick : public LUBitStream {
		LWOOBJID playerID{};
		uint32_t unknown{};
		LUWString kickedPlayer{ 33 };

		GuildKick() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GUILD_KICK) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat: change a member's rank (eGuildRank; LEADER hands the guild over)
	struct GuildSetRank : public LUBitStream {
		LWOOBJID playerID{};
		LUWString targetPlayer{ 33 };
		uint8_t rank{};

		GuildSetRank() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GUILD_SET_RANK) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> chat: the leader disbands the guild
	struct GuildDisband : public LUBitStream {
		LWOOBJID playerID{};

		GuildDisband() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GUILD_DISBAND) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Chat -> the character's world: its guild changed; the world puts it in the character component (guild 0: none).
	// guildName is what other players may see (empty while the name waits for moderation).
	struct GuildStatus : public LUBitStream {
		LWOOBJID characterID{};
		LWOOBJID guildID{};
		LUWString guildName{ 31 };

		GuildStatus() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GUILD_GET_STATUS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	/**
	 * Activity matchmaking (docs/Matchmaking.md). The chat server keeps the lobbies of every world, so players on
	 * different instances of a zone are matched into the same activity instance.
	 */
	enum class eMatchRequestType : int32_t {
		JOIN = 0,  // the client's MatchRequest type 0
		READY = 1, // the client's MatchRequest type 1 (value: ready or not)
		LEAVE = 2, // DLU: the player left the lobby (LobbyExit) or can't stay in it
	};

	// World -> chat: a player joins, readies in or leaves an activity lobby. On a join the world sends what it read
	// from the activity (CDClient Activities, with its own overrides such as solo racing), so chat needs no CDClient.
	struct MatchRequest : public LUBitStream {
		LWOOBJID playerID{};
		eMatchRequestType type{};
		int32_t value{}; // READY: 1 ready, 0 not ready
		int32_t activityID{};
		std::string playerName;    // u32 length, then 1 byte per character
		std::string playerChoices; // the client's name-value text (like "droppedItem=13:<id>"), u32 length then bytes
		uint32_t instanceMapID{};
		int32_t minTeams{};
		int32_t maxTeams{};
		int32_t minTeamSize{};
		int32_t maxTeamSize{};
		int32_t waitTime{};   // milliseconds
		int32_t startDelay{}; // milliseconds

		MatchRequest() : LUBitStream(ServiceType::CHAT, MessageType::Chat::MATCH_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Chat -> world (DLU): a lobby's match has its instance; send these players (the ones in this world) there
	struct MatchTransfer : public LUBitStream {
		// More players than this in one packet is a bad packet
		static constexpr uint32_t MAX_PLAYERS = 64;

		int32_t activityID{};
		LWOZONEID zoneID{};   // map, instance and clone of the activity instance
		std::string serverIP; // u32 length, then 1 byte per character
		uint16_t serverPort{};
		bool mythranShift{};
		std::vector<LWOOBJID> players; // u32 count

		MatchTransfer() : LUBitStream(ServiceType::CHAT, MessageType::Chat::MATCH_TRANSFER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	/**
	 * Chat-service packets the client receives, from the chat server (routed through the player's world) or from its world.
	 */
	namespace Client {
		// Chat in a channel (world -> client; sent to everyone)
		struct GeneralChatMessage : public LUBitStream {
			uint64_t unknown{}; // always 0 from the server
			uint8_t chatChannel{};
			LUWString senderName;
			LWOOBJID senderID{};
			uint16_t sourceID{};
			uint8_t senderGMLevel{}; // DLU always sends 0
			std::u16string message; // u32 length (without the terminator), the characters, then a 0 terminator

			GeneralChatMessage() : LUBitStream(ServiceType::CHAT, MessageType::Chat::GENERAL_CHAT_MESSAGE) {}
			void Serialize(RakNet::BitStream& bitStream) const override;
			bool Deserialize(RakNet::BitStream& bitStream) override;
		};

		// A whisper, or team chat, or why a whisper didn't go out
		struct PrivateChatMessage : public LUBitStream {
			LWOOBJID playerID{}; // the sender again
			eChatChannel chatChannel{};
			uint32_t messageLength{}; // DLU always sends 0
			LUWString senderName;
			LWOOBJID senderID{};
			uint16_t sourceID{};
			eGameMasterLevel senderGMLevel{};
			LUWString receiverName;
			eGameMasterLevel receiverGMLevel{};
			eChatMessageResponseCode responseCode{};
			// Everything that is left. The sender's message as it came in, with its padding.
			LUWString message{ 0u };

			PrivateChatMessage() : LUBitStream(ServiceType::CHAT, MessageType::Chat::PRIVATE_CHAT_MESSAGE) {}
			void Serialize(RakNet::BitStream& bitStream) const override;
			bool Deserialize(RakNet::BitStream& bitStream) override;
		};
	}

	// A system message (chat channel 4) to one client, or everyone when sysAddr is UNASSIGNED_SYSTEM_ADDRESS.
	// `broadcast` is not used: a message with an address only goes to that client.
	void SendSystemMessage(const SystemAddress& sysAddr, const std::string& message, bool broadcast = false);
	void SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, bool broadcast = false);
};

#endif // CHATPACKETS_H
