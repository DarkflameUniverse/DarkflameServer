#include "WorldRoutePacket.h"

namespace ChatPackets {
	void WorldRoutePacket::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(targetID);
		if (routed) routed->WritePacket(bitStream);
		else if (!routedData.empty()) bitStream.WriteAlignedBytes(routedData.data(), static_cast<uint32_t>(routedData.size()));
	}

	bool WorldRoutePacket::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(targetID));
		// Whole bytes only, as they are passed on
		routedData.resize(BITS_TO_BYTES(bitStream.GetNumberOfUnreadBits()));
		if (bitStream.GetNumberOfUnreadBits() % 8 != 0) routedData.pop_back();
		return routedData.empty() || bitStream.ReadAlignedBytes(routedData.data(), static_cast<uint32_t>(routedData.size()));
	}
}
