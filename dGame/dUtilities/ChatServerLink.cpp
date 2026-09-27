#include "ChatServerLink.h"

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "Game.h"
#include "RakPeerInterface.h"

void ChatServerLink::Send(const LUBitStream& msg, const PacketPriority priority, const PacketReliability reliability) {
	if (!Game::chatServer) return;
	RakNet::BitStream bitStream;
	msg.WritePacket(bitStream);
	Game::chatServer->Send(&bitStream, priority, reliability, 0, Game::chatSysAddr, false);
}
