#ifndef MASTERPACKETS_H
#define MASTERPACKETS_H

#include <cstdint>
#include <string>
#include "RakNetTypes.h"
#include "dCommonVars.h"
class TransportLayer;

namespace MasterPackets {
	void SendPersistentIDRequest(TransportLayer* server, uint64_t requestID); //Called from the World server
	void SendPersistentIDResponse(TransportLayer* server, const SystemAddress& sysAddr, uint64_t requestID, uint32_t objID);

	void SendZoneTransferRequest(TransportLayer* server, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t cloneID);
	void SendZoneTransferResponse(TransportLayer* server, const SystemAddress& sysAddr, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, const std::string& serverIP, uint32_t serverPort);

	void HandleServerInfo(Packet* packet);
	void SendServerInfo(TransportLayer* server, Packet* packet);

	void SendZoneCreatePrivate(TransportLayer* server, uint32_t zoneID, uint32_t cloneID, const std::string& password);

	void SendZoneRequestPrivate(TransportLayer* server, uint64_t requestID, bool mythranShift, const std::string& password);

	void SendWorldReady(TransportLayer* server, LWOMAPID zoneId, LWOINSTANCEID instanceId);

	void HandleSetSessionKey(Packet* packet);
}

#endif // MASTERPACKETS_H
