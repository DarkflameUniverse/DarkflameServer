#ifndef MASTERPACKETS_H
#define MASTERPACKETS_H

#include <cstdint>
#include <string>
#include "RakNetTypes.h"
#include "dCommonVars.h"
class dServer;

namespace MasterPackets {
	void SendPersistentIDRequest(dServer* server, uint64_t requestID); //Called from the World server
	void SendPersistentIDResponse(dServer* server, const SystemAddress& sysAddr, uint64_t requestID, uint32_t objID);

	void SendZoneTransferRequest(dServer* server, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t cloneID);
	void SendZoneTransferResponse(dServer* server, const SystemAddress& sysAddr, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, const std::string& serverIP, uint32_t serverPort);

	void HandleServerInfo(Packet* packet);
	void SendServerInfo(dServer* server, Packet* packet);

	void SendZoneCreatePrivate(dServer* server, uint32_t zoneID, uint32_t cloneID, const std::string& password);

	void SendZoneRequestPrivate(dServer* server, uint64_t requestID, bool mythranShift, const std::string& password);

	void SendWorldReady(dServer* server, LWOMAPID zoneId, LWOINSTANCEID instanceId);

	void HandleSetSessionKey(Packet* packet);
}

#endif // MASTERPACKETS_H
