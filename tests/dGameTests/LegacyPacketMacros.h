#ifndef LEGACYPACKETMACROS_H
#define LEGACYPACKETMACROS_H

// FROZEN - DO NOT EDIT.
// The packet macros and the free BitStreamUtils::WriteHeader the hand written packet code used (dCommonVars.h and
// dNet/BitStreamUtils.h before the struct conversion). They were removed from the server once every packet became a
// struct; the frozen oracles in the Legacy/ folders still use them, verbatim.

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "Game.h"
#include "MessageIdentifiers.h"
#include "MessageType/Client.h"
#include "ServiceType.h"
#include "dServer.h"

#ifndef HEADER_SIZE
#define HEADER_SIZE 8
#endif
#ifndef CBITSTREAM
#define CBITSTREAM RakNet::BitStream bitStream;
#define CINSTREAM RakNet::BitStream inStream(packet->data, packet->length, false);
#define CINSTREAM_SKIP_HEADER CINSTREAM if (inStream.GetNumberOfUnreadBits() >= BYTES_TO_BITS(HEADER_SIZE)) inStream.IgnoreBytes(HEADER_SIZE); else inStream.IgnoreBits(inStream.GetNumberOfUnreadBits());
#define CMSGHEADER BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::GAME_MSG);
#define SEND_PACKET Game::server->Send(bitStream, sysAddr, false);
#define SEND_PACKET_BROADCAST Game::server->Send(bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
#endif

#ifndef BITSTREAMUTILS_HAS_WRITEHEADER
namespace BitStreamUtils {
	template<typename T>
	void WriteHeader(RakNet::BitStream& bitStream, ServiceType connectionType, T internalPacketID) {
		bitStream.Write<MessageID>(ID_USER_PACKET_ENUM);
		bitStream.Write<ServiceType>(connectionType);
		bitStream.Write(static_cast<uint32_t>(internalPacketID));
		bitStream.Write<uint8_t>(0);
	}
}
#endif

#endif // LEGACYPACKETMACROS_H
