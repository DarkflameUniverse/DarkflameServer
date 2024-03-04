#include "MasterPackets.h"
#include "BitStream.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "eConnectionType.h"
#include "eMasterMessageType.h"
#include "BitStreamUtils.h"

#include <string>

void MasterPackets::SendPersistentIDRequest(dServer* server, uint64_t requestID) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::REQUEST_PERSISTENT_ID);
	bitStream.Write(requestID);
	server->SendToMaster(bitStream);
}

void MasterPackets::SendPersistentIDResponse(dServer* server, const SystemAddress& sysAddr, uint64_t requestID, uint32_t objID) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::REQUEST_PERSISTENT_ID_RESPONSE);

	bitStream.Write(requestID);
	bitStream.Write(objID);

	server->Send(bitStream, sysAddr, false);
}

void MasterPackets::SendZoneTransferRequest(dServer* server, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t cloneID) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::REQUEST_ZONE_TRANSFER);

	bitStream.Write(requestID);
	bitStream.Write<uint8_t>(mythranShift);
	bitStream.Write(zoneID);
	bitStream.Write(cloneID);

	server->SendToMaster(bitStream);
}

void MasterPackets::SendZoneCreatePrivate(dServer* server, uint32_t zoneID, uint32_t cloneID, const std::string& password) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::CREATE_PRIVATE_ZONE);

	bitStream.Write(zoneID);
	bitStream.Write(cloneID);

	bitStream.Write<uint32_t>(password.size());
	for (auto character : password) {
		bitStream.Write<char>(character);
	}

	server->SendToMaster(bitStream);
}

void MasterPackets::SendZoneRequestPrivate(dServer* server, uint64_t requestID, bool mythranShift, const std::string& password) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::REQUEST_PRIVATE_ZONE);

	bitStream.Write(requestID);
	bitStream.Write<uint8_t>(mythranShift);

	bitStream.Write<uint32_t>(password.size());
	for (auto character : password) {
		bitStream.Write<char>(character);
	}

	server->SendToMaster(bitStream);
}

void MasterPackets::SendWorldReady(dServer* server, LWOMAPID zoneId, LWOINSTANCEID instanceId) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::WORLD_READY);

	bitStream.Write(zoneId);
	bitStream.Write(instanceId);

	server->SendToMaster(bitStream);
}

void MasterPackets::SendZoneTransferResponse(dServer* server, const SystemAddress& sysAddr, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, const std::string& serverIP, uint32_t serverPort) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::REQUEST_ZONE_TRANSFER_RESPONSE);

	bitStream.Write(requestID);
	bitStream.Write<uint8_t>(mythranShift);
	bitStream.Write(zoneID);
	bitStream.Write(zoneInstance);
	bitStream.Write(zoneClone);
	bitStream.Write<uint16_t>(serverPort);
	bitStream.Write(LUString(serverIP, 255));

	server->Send(bitStream, sysAddr, false);
}

void MasterPackets::HandleServerInfo(Packet* packet) {
	RakNet::BitStream inStream(packet->data, packet->length, false);
	uint64_t header = inStream.Read(header);

	uint32_t theirPort = 0;
	uint32_t theirZoneID = 0;
	uint32_t theirInstanceID = 0;
	LUString theirIP;

	inStream.Read(theirPort);
	inStream.Read(theirZoneID);
	inStream.Read(theirInstanceID);
	inStream.Read(theirIP);

	//TODO: Actually mark this server as an available server in the manager
}

void MasterPackets::SendServerInfo(dServer* server, Packet* packet) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::SERVER_INFO);

	bitStream.Write(server->GetPort());
	bitStream.Write(server->GetZoneID());
	bitStream.Write(server->GetInstanceID());
	bitStream.Write(server->GetServerType());
	bitStream.Write(LUString(server->GetIP()));

	server->SendToMaster(bitStream);
}
