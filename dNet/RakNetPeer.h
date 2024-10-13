#pragma once

#include "TransportPeerInterface.h"

class RakNetPeer : public TransportPeerInterface {
public:
	RakNetPeer(RakPeerInterface* peer, const std::string& ip, int port, const std::string& password);
	~RakNetPeer();

	void Send(
		const RakNet::BitStream* bitStream,
		PacketPriority priority,
		PacketReliability reliability,
		char orderingChannel,
		SystemAddress systemAddress,
		bool broadcast
	) override;

	void Disconnect() override;

	void Reconnect() override;

	Packet* Receive() override;

	void DeallocatePacket(Packet* packet) override;

private:
	RakPeerInterface* m_Peer;
	std::string m_IP;
	int m_Port;
	std::string m_Password;
};