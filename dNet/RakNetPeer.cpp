#include "RakNetPeer.h"

#include "RakPeerInterface.h"

RakNetPeer::RakNetPeer(RakPeerInterface* peer, const std::string& ip, int port, const std::string& password)
{
	m_Peer = peer;
	m_IP = ip;
	m_Port = port;
	m_Password = password;
}

RakNetPeer::~RakNetPeer()
{
	delete m_Peer;
}

void RakNetPeer::Send(
	const RakNet::BitStream* bitStream,
	PacketPriority priority,
	PacketReliability reliability,
	char orderingChannel,
	SystemAddress systemAddress,
	bool broadcast
)
{
	m_Peer->Send(bitStream, priority, reliability, orderingChannel, systemAddress, broadcast);
}

void RakNetPeer::Disconnect()
{
	m_Peer->Shutdown(0);
}

void RakNetPeer::Reconnect()
{
	m_Peer->Connect(m_IP.c_str(), m_Port, m_Password.c_str(), m_Password.length());
}

Packet* RakNetPeer::Receive()
{
	return m_Peer->Receive();
}

void RakNetPeer::DeallocatePacket(Packet* packet)
{
	m_Peer->DeallocatePacket(packet);
}
