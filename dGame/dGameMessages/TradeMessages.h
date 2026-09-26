#ifndef TRADEMESSAGES_H
#define TRADEMESSAGES_H

#include "GameMessages.h"

#include <optional>
#include <vector>

// Game messages for player to player trading.
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
// The server -> client messages are sent with Send (UNASSIGNED_SYSTEM_ADDRESS broadcasts), as before.
namespace GameMessages {
	// One entry of a trade offer: the object ID it is keyed by, then the client's inventory item layout.
	// Optional fields are written as a flag bit followed by the value when the flag is set.
	struct TradeItemEntry {
		void Serialize(RakNet::BitStream& bitStream) const;
		bool Deserialize(RakNet::BitStream& bitStream);
		bool operator==(const TradeItemEntry&) const = default;

		LWOOBJID key{};    // the map key, the item's object ID
		LWOOBJID itemID{};
		LOT templateID{};
		std::optional<LWOOBJID> subkey{};
		std::optional<uint32_t> count{};
		std::optional<uint16_t> slot{};
		std::optional<uint32_t> inventoryType{};

		// Optional config (LDF) block, kept as bytes: u32 uncompressed size, compressed bit, then either the raw
		// data (uncompressed size bytes) or a u32 compressed size and the compressed data.
		struct Config {
			uint32_t size{};
			bool compressed{};
			uint32_t compressedSize{};
			std::vector<uint8_t> data{};
			bool operator==(const Config&) const = default;
		};
		std::optional<Config> config{};

		bool unknownFlag{}; // trailing bit, not used by DLU
	};

	// Client -> server.
	struct ClientTradeRequest : public NetGameMsg {
		ClientTradeRequest() : NetGameMsg(MessageType::Game::CLIENT_TRADE_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bNeedInvitePopUp{};
		LWOOBJID i64Invitee{};
	};

	// Server -> client.
	struct ServerTradeInvite : public NetGameMsg {
		ServerTradeInvite() : NetGameMsg(MessageType::Game::SERVER_TRADE_INVITE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bNeedInvitePopUp{};
		LWOOBJID i64Requestor{};
		std::u16string wsName{}; // u32 length prefixed
	};

	// Server -> client.
	struct ServerTradeInitialReply : public NetGameMsg {
		ServerTradeInitialReply() : NetGameMsg(MessageType::Game::SERVER_TRADE_INITIAL_REPLY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID i64Invitee{};
		int32_t resultType{};
		std::u16string wsName{}; // u32 length prefixed
	};

	// Server -> client.
	struct ServerTradeFinalReply : public NetGameMsg {
		ServerTradeFinalReply() : NetGameMsg(MessageType::Game::SERVER_TRADE_FINAL_REPLY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bResult{};
		LWOOBJID i64Invitee{};
		std::u16string wsName{}; // u32 length prefixed
	};

	// Client -> server.
	struct ClientTradeUpdate : public NetGameMsg {
		ClientTradeUpdate() : NetGameMsg(MessageType::Game::CLIENT_TRADE_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		uint64_t i64Currency{};
		std::vector<TradeItemEntry> inventoryMap{}; // u32 count, then the entries
	};

	// Server -> client.
	struct ServerTradeUpdate : public NetGameMsg {
		ServerTradeUpdate() : NetGameMsg(MessageType::Game::SERVER_TRADE_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bAboutToPerform{};
		uint64_t i64Currency{};
		std::vector<TradeItemEntry> inventoryMap{}; // u32 count, then the entries
	};

	// Client -> server. No payload.
	struct ClientTradeCancel : public NetGameMsg {
		ClientTradeCancel() : NetGameMsg(MessageType::Game::CLIENT_TRADE_CANCEL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Client -> server.
	struct ClientTradeAccept : public NetGameMsg {
		ClientTradeAccept() : NetGameMsg(MessageType::Game::CLIENT_TRADE_ACCEPT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bFirst{};
	};

	// Server -> client. No payload.
	struct ServerTradeCancel : public NetGameMsg {
		ServerTradeCancel() : NetGameMsg(MessageType::Game::SERVER_TRADE_CANCEL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client.
	struct ServerTradeAccept : public NetGameMsg {
		ServerTradeAccept() : NetGameMsg(MessageType::Game::SERVER_TRADE_ACCEPT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bFirst{};
	};
};

#endif // TRADEMESSAGES_H
