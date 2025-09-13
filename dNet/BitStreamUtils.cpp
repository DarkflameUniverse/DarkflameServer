#include "BitStreamUtils.h"
#include "dServer.h"
#include "BitStream.h"
#include "PacketUtils.h"


void LUBitStream::WriteHeader(RakNet::BitStream& bitStream) const {
	bitStream.Write<MessageID>(ID_USER_PACKET_ENUM);
	bitStream.Write(this->serviceType);
}

bool LUBitStream::ReadHeader(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(this->rakNetID));
	VALIDATE_READ(bitStream.Read(this->serviceType));
	return true;
}

void LUBitStream::Send(const SystemAddress& sysAddr) const {
	RakNet::BitStream bitStream;
	this->WriteHeader(bitStream);
	this->Serialize(bitStream);
	Game::server->Send(bitStream, sysAddr, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);
}
