#ifndef COMMONPACKETS_H
#define COMMONPACKETS_H

#include "BitStreamUtils.h"
#include "MessageType/Server.h"
#include "RakNetTypes.h"

#include <cstdint>

enum class eServerDisconnectIdentifiers : uint32_t;

// Packets of ServiceType::COMMON (the client's MSG_SERVER_* family), used by every server the client talks to.
namespace CommonPackets {
	// Client -> server. The first packet on every connection.
	struct ClientVersionConfirm : public LUBitStream {
		// Set by the dispatcher before Deserialize and Handle.
		SystemAddress sysAddr = UNASSIGNED_SYSTEM_ADDRESS;

		uint32_t netVersion{};
		uint32_t unknown{}; // Not used by DLU
		ServiceType serviceType{};
		uint16_t padding{};
		uint32_t processID{};
		uint16_t port{};
		LUString unknown2{ 33 }; // Not used by DLU; lu_packets calls it padding

		ClientVersionConfirm() : LUBitStream(ServiceType::COMMON, MessageType::Server::VERSION_CONFIRM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		// Logs what the client sent and answers with a ServerVersionConfirm for this server.
		void Handle() override;
	};

	// Server -> client. The answer to ClientVersionConfirm.
	// The client (ServerHandShakePacket, 1.10.64) only reads netVersion, unknown and serviceType; the trailing
	// 8 bytes are what DLU has always sent after them.
	struct ServerVersionConfirm : public LUBitStream {
		static constexpr uint32_t DEFAULT_NET_VERSION = 171022;
		static constexpr uint32_t UNKNOWN_VALUE = 861228100;
		static constexpr uint64_t TRAILING_VALUE = 219818307120;

		uint32_t netVersion = DEFAULT_NET_VERSION;
		uint32_t unknown = UNKNOWN_VALUE;
		uint32_t serviceType{}; // The server's ServiceType, written as 4 bytes
		uint64_t trailing = TRAILING_VALUE;

		ServerVersionConfirm() : LUBitStream(ServiceType::COMMON, MessageType::Server::VERSION_CONFIRM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client. Tells the client why it is about to be disconnected.
	struct DisconnectNotify : public LUBitStream {
		eServerDisconnectIdentifiers disconnectID{};

		DisconnectNotify() : LUBitStream(ServiceType::COMMON, MessageType::Server::DISCONNECT_NOTIFY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Server -> client. Layout from the 1.10.64 client (PacketHandler_MSG_SERVER_GENERAL_NOTIFY, ServerGeneralNotifyPacket):
	// notifyType 0 is "just logged off a duplicate account"; showMessageBox shows the localized text. DLU does not send it yet.
	struct GeneralNotify : public LUBitStream {
		int32_t notifyType{};
		bool showMessageBox{}; // Written as one byte

		GeneralNotify() : LUBitStream(ServiceType::COMMON, MessageType::Server::GENERAL_NOTIFY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Handles a ServiceType::COMMON packet whose header has already been read from inStream.
	void Handle(RakNet::BitStream& inStream, const SystemAddress& sysAddr, uint32_t packetID);
}

#endif // COMMONPACKETS_H
