#include "ChatServerLink.h"

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "Game.h"
#include "RakPeerInterface.h"
#include "TrafficStats.h"

void ChatServerLink::Send(const LUBitStream& msg, const PacketPriority priority, const PacketReliability reliability) {
	if (!Game::chatServer) return;
	RakNet::BitStream bitStream;
	msg.WritePacket(bitStream);
	const auto bytes = bitStream.GetNumberOfBytesUsed();
	TrafficStats::Local().Packet(TrafficStats::Now(), TrafficStats::KeyOf(bitStream.GetData(), bytes, true), bytes, 1, TrafficStats::Peer::SERVERS);
	Game::chatServer->Send(&bitStream, priority, reliability, 0, Game::chatSysAddr, false);
}

void ChatServerLink::CountReceived(const unsigned char* data, unsigned int length) {
	TrafficStats::Local().Packet(TrafficStats::Now(), TrafficStats::KeyOf(data, length, false), length, 1, TrafficStats::Peer::SERVERS);
}
