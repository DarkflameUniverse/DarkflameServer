#ifndef PACKETDISPATCHER_H
#define PACKETDISPATCHER_H

#include <functional>
#include <map>
#include <memory>
#include <type_traits>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "Logger.h"
#include "RakNetTypes.h"
#include "StringifiedEnum.h"

/**
 * One dispatch map per service a server receives (see docs/PacketArchitecture.md, section 2.2).
 *
 * Each entry names the struct a packet ID is read into and the function that handles it, so the same struct can be
 * handled differently by each server that receives it (WORLD_READY means one thing to master and another to the
 * dashboard). Dispatch reads the packet into a fresh struct; a packet that fails to Deserialize is logged and
 * dropped before its handler runs.
 *
 *     PacketDispatcher<MessageType::Master> handlers;
 *     handlers.On<MasterPackets::PlayerAdded>(MessageType::Master::PLAYER_ADDED, &OnPlayerAdded);
 *     ...
 *     handlers.Dispatch(packet); // reads the header and routes by packet ID
 */
template<typename IdT>
class PacketDispatcher {
public:
	template<typename Msg>
	using Handler = std::function<void(Msg& msg, const SystemAddress& sysAddr)>;

	// Registers the struct packets with this ID are read into, and what to do with them
	template<typename Msg>
	void On(const IdT id, Handler<Msg> handler) {
		static_assert(std::is_base_of_v<LUBitStream, Msg>, "packets are LUBitStream structs");
		m_Handlers[id] = [handler = std::move(handler)](RakNet::BitStream& bitStream, const SystemAddress& sysAddr) {
			Msg msg;
			if (!msg.Deserialize(bitStream)) return false;
			handler(msg, sysAddr);
			return true;
		};
	}

	bool Has(const IdT id) const { return m_Handlers.contains(id); }

	/**
	 * Routes a payload (the stream positioned right after the packet header) to its handler.
	 * Returns false if the ID has no handler. Packets that fail to Deserialize are logged and dropped.
	 */
	bool Dispatch(const IdT id, RakNet::BitStream& bitStream, const SystemAddress& sysAddr) const {
		const auto it = m_Handlers.find(id);
		if (it == m_Handlers.end()) return false;
		if (!it->second(bitStream, sysAddr)) {
			LOG("Dropped a %s packet (%u) from %s that failed to read", StringifiedEnum::ToString(id).data(), static_cast<uint32_t>(id), sysAddr.ToString());
		}
		return true;
	}

	/**
	 * Reads the packet's header and routes the rest to the handler for its ID. Returns false if the packet is not an
	 * LU packet of this dispatcher's service or has no handler.
	 */
	bool Dispatch(const Packet* packet, const ServiceType service) const {
		if (!packet || packet->length < 1) return false;
		RakNet::BitStream bitStream(packet->data, packet->length, false);
		LUBitStream header;
		if (!header.ReadHeader(bitStream) || header.connectionType != service) return false;
		return Dispatch(static_cast<IdT>(header.internalPacketID), bitStream, packet->systemAddress);
	}

private:
	std::map<IdT, std::function<bool(RakNet::BitStream&, const SystemAddress&)>> m_Handlers;
};

#endif // PACKETDISPATCHER_H
