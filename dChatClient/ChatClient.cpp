#include <iostream>
#include <RakPeerInterface.h>
#include <RakNetworkFactory.h>
#include <MessageIdentifiers.h>
#include <BitStream.h>

#include "eConnectionType.h"
#include "eChatMessageType.h"
#include "dCommonVars.h"

static constexpr uint16_t CHAT_PORT = 2005;
static const char PASS_INTERNAL[16] = "3.25 DARKFLAME1";
static const char PASS_EXTERNAL[9] = "3.25 ND1";

int main(int argc, const char** argv) {
	std::cout << "Hello World!" << std::endl;

	SocketDescriptor socketDescriptor(0, 0);
	RakPeerInterface* peer = RakNetworkFactory::GetRakPeerInterface();
	uint16_t maxConnections = 1; // one outgoing
	bool useEncryption = true;

	if (!peer) {
		std::cerr << "Failed to get RakPeer interface!" << std::endl;
		return 1;
	}
	if (!peer->Startup(maxConnections, 10, &socketDescriptor, 1)) {
		std::cerr << "Failed to startup rak peer interface!" << std::endl;
		return 1;
	}

	if (useEncryption) peer->InitializeSecurity(NULL, NULL, NULL, NULL);

	if (!peer->Connect("localhost", CHAT_PORT, PASS_EXTERNAL, 8)) {
		std::cerr << "Failed to initiate connection to chat server" << std::endl;
		return 1;
	}

	// Establish connection
	Packet* packet;
	bool connected = false;
	SystemAddress remote;

	while (!connected) {
		packet = peer->Receive();
		if (!packet) continue;

		uint8_t packet_id = packet->data[0];
		switch (packet_id) {
		case ID_INVALID_PASSWORD:
			std::cerr << "Password invalid" << std::endl;
			return 1;
		case ID_CONNECTION_REQUEST_ACCEPTED:
			std::cout << "Connection accepted" << std::endl;
			remote = packet->systemAddress;
			connected = true;
			break;
		default:
			std::cout << "Packet: " << static_cast<uint32_t>(packet_id) << std::endl;
		}
		peer->DeallocatePacket(packet);
	}

	std::cout << "Starting tests" << std::endl;

	//Notify chat about it
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::GM_ANNOUNCE);

	std::string title = "My Title";
	std::string message = "My Message";

	bitStream.Write<uint32_t>(title.size());
	bitStream.Write(title.c_str(), title.size());
	bitStream.Write<uint32_t>(message.size());
	bitStream.Write(message.c_str(), message.size());

	peer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, remote, false);

	while (true) {
		packet = peer->Receive();
		if (!packet) continue;

		uint8_t packet_id = packet->data[0];
		switch (packet_id) {
		default:
			std::cout << "Packet: " << static_cast<uint32_t>(packet_id) << std::endl;
		}
		peer->DeallocatePacket(packet);
	}

	return 0;
}
