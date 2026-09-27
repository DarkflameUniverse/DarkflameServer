#ifndef CHATSERVERLINK_H
#define CHATSERVERLINK_H

#include "PacketPriority.h"

struct LUBitStream;

// A world's connection to the chat server
namespace ChatServerLink {
	// World -> chat: sends msg (a ChatPackets struct) over the world's chat connection
	void Send(const LUBitStream& msg, PacketPriority priority = SYSTEM_PRIORITY, PacketReliability reliability = RELIABLE);
}

#endif // CHATSERVERLINK_H
