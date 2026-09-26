#ifndef MASTERPACKETS_H
#define MASTERPACKETS_H

#include <cstdint>
#include <string>
#include "RakNetTypes.h"
#include "dCommonVars.h"
#include "Stamps.h"
class dServer;

namespace MasterPackets {
	// stamps: the login stamps travelling with the request (empty unless auth asks for a world during a login)
	void SendZoneTransferRequest(dServer* server, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t cloneID, const Stamps& stamps = {});
	void SendZoneTransferResponse(dServer* server, const SystemAddress& sysAddr, uint64_t requestID, bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, const std::string& serverIP, uint32_t serverPort, const Stamps& stamps = {});

	void HandleServerInfo(Packet* packet);
	void SendServerInfo(dServer* server, Packet* packet);

	void SendZoneCreatePrivate(dServer* server, uint32_t zoneID, uint32_t cloneID, const std::string& password);

	void SendZoneRequestPrivate(dServer* server, uint64_t requestID, bool mythranShift, const std::string& password);

	void SendWorldReady(dServer* server, LWOMAPID zoneId, LWOINSTANCEID instanceId);
}

#endif // MASTERPACKETS_H
