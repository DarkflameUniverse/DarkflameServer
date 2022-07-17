#include "MasterPackets.h"
#include "BitStream.h"
#include "PacketUtils.h"
#include "dMessageIdentifiers.h"
#include "dCommonVars.h"
#include "dServer.h"

#include <string>

void MasterPackets::SendPersistentIDRequest(dServer* server, uint64_t requestID) {
	CBITSTREAM
    PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_REQUEST_PERSISTENT_ID);
    bitStream.Write(requestID);
	server->SendToMaster(&bitStream);
}

void MasterPackets::SendPersistentIDResponse(dServer* server, const SystemAddress& sysAddr, uint64_t requestID, uint32_t objID) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_REQUEST_PERSISTENT_ID_RESPONSE);
	
	bitStream.Write(requestID);
	bitStream.Write(objID);
	
	server->Send(&bitStream, sysAddr, false);
}

void MasterPackets::SendZoneTransferRequest(dServer* server, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t cloneID) {
	RakNet::BitStream bitStream;
    PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_REQUEST_ZONE_TRANSFER);
    
    bitStream.Write(requestID);
    bitStream.Write(static_cast<uint8_t>(mythranShift));
    bitStream.Write(zoneID);
    bitStream.Write(cloneID);
    
	server->SendToMaster(&bitStream);
}

void MasterPackets::SendZoneCreatePrivate(dServer* server, uint32_t zoneID, uint32_t cloneID, const std::string& password) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_CREATE_PRIVATE_ZONE);

	bitStream.Write(zoneID);
	bitStream.Write(cloneID);

	bitStream.Write<uint32_t>(password.size());
	for (auto character : password) {
		bitStream.Write<char>(character);
	}

	server->SendToMaster(&bitStream);
}

void MasterPackets::SendZoneRequestPrivate(dServer* server, uint64_t requestID, bool mythranShift, const std::string& password) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_REQUEST_PRIVATE_ZONE);

	bitStream.Write(requestID);
	bitStream.Write(static_cast<uint8_t>(mythranShift));

	bitStream.Write<uint32_t>(password.size());
	for (auto character : password) {
		bitStream.Write<char>(character);
	}

	server->SendToMaster(&bitStream);
}

void MasterPackets::SendWorldReady(dServer* server, LWOMAPID zoneId, LWOINSTANCEID instanceId)
{
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_WORLD_READY);

	bitStream.Write(zoneId);
	bitStream.Write(instanceId);
	
	server->SendToMaster(&bitStream);
}

void MasterPackets::SendZoneTransferResponse(dServer* server, const SystemAddress& sysAddr, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, const std::string& serverIP, uint32_t serverPort) {
    RakNet::BitStream bitStream;
    PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_REQUEST_ZONE_TRANSFER_RESPONSE);
    
    bitStream.Write(requestID);
    bitStream.Write(static_cast<uint8_t>(mythranShift));
    bitStream.Write(zoneID);
    bitStream.Write(zoneInstance);
    bitStream.Write(zoneClone);
    bitStream.Write(static_cast<uint16_t>(serverPort));
    PacketUtils::WriteString(bitStream, serverIP, static_cast<uint32_t>(serverIP.size() + 1));
    
    server->Send(&bitStream, sysAddr, false);
}

void MasterPackets::HandleServerInfo(Packet* packet) {
	RakNet::BitStream inStream(packet->data, packet->length, false);
	uint64_t header = inStream.Read(header);
	
	uint32_t theirPort = 0;
	uint32_t theirZoneID = 0;
	uint32_t theirInstanceID = 0;
	std::string theirIP = "";
	
	inStream.Read(theirPort);
	inStream.Read(theirZoneID);
	inStream.Read(theirInstanceID);
	theirIP = PacketUtils::ReadString(inStream.GetReadOffset(), packet, false); //20 is the current offset
	
	//TODO: Actually mark this server as an available server in the manager
}

void MasterPackets::SendServerInfo(dServer* server, Packet* packet) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_SERVER_INFO);
	
	bitStream.Write(server->GetPort());
	bitStream.Write(server->GetZoneID());
	bitStream.Write(server->GetInstanceID());
	bitStream.Write(server->GetServerType());
	PacketUtils::WriteString(bitStream, server->GetIP(), server->GetIP().size());
	
	server->SendToMaster(&bitStream);
}
