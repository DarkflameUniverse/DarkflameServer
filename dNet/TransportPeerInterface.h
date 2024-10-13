#pragma once

#include <string>
#include "RakNetTypes.h"
#include "PacketPriority.h"

class TransportPeerInterface {
public:
	virtual void Send(
		const RakNet::BitStream* bitStream,
		PacketPriority priority,
		PacketReliability reliability,
		char orderingChannel,
		SystemAddress systemAddress,
		bool broadcas
	) = 0;

	virtual void Disconnect() = 0;

	virtual void Reconnect() = 0;

	virtual Packet* Receive() = 0;

	virtual void DeallocatePacket(Packet* packet) = 0;
};
