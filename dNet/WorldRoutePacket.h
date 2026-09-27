#ifndef WORLDROUTEPACKET_H
#define WORLDROUTEPACKET_H

#include <cstdint>
#include <vector>

#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "MessageType/Chat.h"

/**
 * WORLD_ROUTE_PACKET (chat service), in its own file because it carries a packet of another service: the chat server
 * wraps what it sends a client (ClientPackets, ChatPackets::Client) in one of these to the world the player is in.
 */
namespace ChatPackets {
	// Chat -> world: pass `routed` to the client of targetID. The world forwards everything after targetID as is.
	struct WorldRoutePacket : public LUBitStream {
		LWOOBJID targetID{};
		// Serialize: the packet to route (its header included). Not owned.
		const LUBitStream* routed{};
		// Deserialize: the routed packet's bytes, header included. Serialize writes these when routed is null.
		std::vector<uint8_t> routedData;

		WorldRoutePacket() : LUBitStream(ServiceType::CHAT, MessageType::Chat::WORLD_ROUTE_PACKET) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

}

#endif // WORLDROUTEPACKET_H
