#ifndef COMMONPACKETS_H
#define COMMONPACKETS_H

#include "BitStreamUtils.h"
#include "BuildInfo.h"
#include "MessageType/Server.h"
#include "RakNetTypes.h"

#include <cstdint>
#include <string>

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
	// The client (1.10.64, PacketHandler_MSG_SERVER_VERSION_CONFIRM) reads only netVersion and serviceType; it never
	// reads unknown and ignores anything after serviceType (no length check). DLU uses those bytes to say which build
	// the server is:
	//   u32 netVersion | u32 unknown ("DLU3") | u32 serviceType
	//   | u8 major | u8 minor | u8 patch | u8 flags (bits 0-1 BuildInfo::eBuildKind, bit 2 dirty)
	//   | 4 bytes: first 32 bits of the commit hash, in hash order (a hex dump shows the hash)
	//   | u16 length + build string, e.g. "3.0.0-experimental+g1a2b3c4d-dirty" (optional on read)
	struct ServerVersionConfirm : public LUBitStream {
		static constexpr uint32_t DEFAULT_NET_VERSION = 171022;
		static constexpr uint32_t UNKNOWN_VALUE = 861228100; // "DLU3"
		static constexpr uint32_t MAX_BUILD_STRING_LENGTH = 1024;

		uint32_t netVersion = DEFAULT_NET_VERSION;
		uint32_t unknown = UNKNOWN_VALUE;
		uint32_t serviceType{}; // The server's ServiceType, written as 4 bytes
		uint8_t versionMajor = BuildInfo::versionMajor;
		uint8_t versionMinor = BuildInfo::versionMinor;
		uint8_t versionPatch = BuildInfo::versionPatch;
		uint8_t buildFlags = BuildInfo::Flags();
		uint32_t commitPrefix = BuildInfo::CommitPrefix();
		std::string buildString{ BuildInfo::buildString };

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
