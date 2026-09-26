#include "BitStreamUtils.h"
#include "dServer.h"
#include "BitStream.h"
#include "PacketUtils.h"


void LUBitStream::WriteHeader(RakNet::BitStream& bitStream) const {
	bitStream.Write<MessageID>(ID_USER_PACKET_ENUM);
	bitStream.Write(this->connectionType);
	bitStream.Write(this->internalPacketID);
	bitStream.Write<uint8_t>(0); // padding
}

bool LUBitStream::ReadHeader(RakNet::BitStream& bitStream) {
	MessageID messageID;
	bitStream.Read(messageID);
	if (messageID != ID_USER_PACKET_ENUM) return false;
	VALIDATE_READ(bitStream.Read(this->connectionType));
	VALIDATE_READ(bitStream.Read(this->internalPacketID));
    bitStream.IgnoreBytes(1);
	return true;
}

void LUBitStream::WritePacket(RakNet::BitStream& bitStream) const {
	this->WriteHeader(bitStream);
	this->Serialize(bitStream);
}

void LUBitStream::Send(const SystemAddress& sysAddr) const {
	RakNet::BitStream bitStream;
	this->WritePacket(bitStream);
	Game::server->Send(bitStream, sysAddr, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);
}
