#ifndef PLAYERMESSAGES_H
#define PLAYERMESSAGES_H

#include "GameMessages.h"
#include "eGameMasterLevel.h"
#include "eHelpType.h"
#include "eLootSourceType.h"

#include <string>

// Game messages about a player's own state: GM level and chat mode, score, coins, reputation, statistics, chat
// commands and bug reports.
// Fields are listed in wire order; names follow the client (legouniverse.exe 1.10.64) where known.
namespace GameMessages {
	// Server -> client, broadcast.
	struct UpdateChatMode : public NetGameMsg {
		UpdateChatMode() : NetGameMsg(MessageType::Game::UPDATE_CHAT_MODE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eGameMasterLevel level{};
	};

	// Server -> client, broadcast.
	struct SetGMLevel : public NetGameMsg {
		SetGMLevel() : NetGameMsg(MessageType::Game::SET_GM_LEVEL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// DLU always writes this bit set.
		bool bOverride{ true };
		eGameMasterLevel level{};
	};

	// Server -> client, to one client.
	struct ModifyLEGOScore : public NetGameMsg {
		ModifyLEGOScore() : NetGameMsg(MessageType::Game::MODIFY_LEGO_SCORE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int64_t score{};
		eLootSourceType sourceType{ eLootSourceType::NONE }; // optional
	};

	// Server -> client, to one client.
	struct SetCurrency : public NetGameMsg {
		SetCurrency() : NetGameMsg(MessageType::Game::SET_CURRENCY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int64_t currency{};
		int32_t lootType{ LOOTTYPE_NONE }; // optional
		NiPoint3 position{ NiPoint3Constant::ZERO };
		LOT sourceLOT{ LOT_NULL }; // optional
		LWOOBJID sourceID{ LWOOBJID_EMPTY }; // optional
		// optional (default 0). DLU writes it as an int32_t; lu_packets has an object ID (8 bytes). Only ever 0.
		int32_t sourceTradeID{ 0 };
		eLootSourceType sourceType{ eLootSourceType::NONE }; // optional
	};

	// Server -> client, to one client.
	struct UpdateReputation : public NetGameMsg {
		UpdateReputation() : NetGameMsg(MessageType::Game::UPDATE_REPUTATION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int64_t reputation{};
	};

	// Client -> server. The client sets or clears a tooltip's bit in its own flags, then sends SetFlag for the same
	// ID; the server keeps the bits and saves them as char@ttip, which the client reads on load.
	struct SetTooltipFlag : public NetGameMsg {
		SetTooltipFlag() : NetGameMsg(MessageType::Game::SET_TOOLTIP_FLAG) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bFlag{};
		int32_t iToolTip{};
	};

	// Server -> client, broadcast.
	struct ToggleGMInvis : public NetGameMsg {
		ToggleGMInvis() : NetGameMsg(MessageType::Game::TOGGLE_GM_INVIS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bStateOut{ false };
	};
	using ToggleGMInvisEvent = NetGameMsgEvent<ToggleGMInvis>;

	// Client -> server. DLU reads only the amount; lu_packets has a position after it.
	struct PickupCurrency : public NetGameMsg {
		PickupCurrency() : NetGameMsg(MessageType::Game::PICKUP_CURRENCY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		uint32_t currency{};
	};

	// Client -> server.
	struct ModifyPlayerZoneStatistic : public NetGameMsg {
		ModifyPlayerZoneStatistic() : NetGameMsg(MessageType::Game::MODIFY_PLAYER_ZONE_STATISTIC) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bSet{ false };
		std::u16string statName{};
		int32_t statValue{ 0 }; // optional
		LWOMAPID zoneID{ LWOMAPID_INVALID }; // optional
	};

	// Server -> client (CharacterComponent::SendPlayerStatistic), as live sent it; live captures hold no client -> server
	// copy, but one is still counted when it comes. The client's CharacterComponent adds updateValue
	// to the passport statistic and, for every statistic but MetersTraveled, updates the passport UI. updateID is a
	// StatisticID. See docs/CaptureUnknowns.md.
	struct UpdatePlayerStatistic : public NetGameMsg {
		UpdatePlayerStatistic() : NetGameMsg(MessageType::Game::UPDATE_PLAYER_STATISTIC) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t updateID{};
		int64_t updateValue{ 1 }; // optional
	};

	// Client -> server. Chat text the client parses as a command (starts with '/').
	struct ParseChatMessage : public NetGameMsg {
		ParseChatMessage() : NetGameMsg(MessageType::Game::PARSE_CHAT_MESSAGE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t iClientState{};
		std::u16string wsString{};
	};

	// Client -> server. A bug report, or a player report when otherPlayerID holds a player's ID.
	struct ReportBug : public NetGameMsg {
		ReportBug() : NetGameMsg(MessageType::Game::REPORT_BUG) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::u16string body{};
		std::string clientVersion{};
		std::string otherPlayerID{};
		std::string selection{};
	};

	// Client -> server. Nothing is done with it.
	struct VerifyAck : public NetGameMsg {
		VerifyAck() : NetGameMsg(MessageType::Game::VERIFY_ACK) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bDifferent{ false };
		std::string sBitStream{};
		uint32_t uiHandle{ 0 }; // optional
	};

	// Server -> client, to the player. Shows a one-time tutorial tooltip (LWOCharacterComponent::ShowHelp); the
	// client skips it once the player flag with the same ID is set, and sets that flag itself.
	// Serialize 0x00dc51e0 / Deserialize 0x00dc5220: a single int32.
	struct Help : public NetGameMsg {
		Help() : NetGameMsg(MessageType::Game::HELP) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eHelpType helpId{ eHelpType::NONE };
	};
}

#endif // PLAYERMESSAGES_H
