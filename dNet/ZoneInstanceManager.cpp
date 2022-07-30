#define _VARIADIC_MAX 10
#include "ZoneInstanceManager.h"

// Custom Classes
#include "MasterPackets.h"
#include "PacketUtils.h"
#include "dServer.h"

// C++
#include <future>

// Static Variables
ZoneInstanceManager* ZoneInstanceManager::m_Address = nullptr;

//! Requests a zone transfer
void ZoneInstanceManager::RequestZoneTransfer(dServer* server, uint32_t zoneID, uint32_t zoneClone, bool mythranShift, std::function<void(bool, uint32_t, uint32_t, uint32_t, std::string, uint16_t)> callback) {

	ZoneTransferRequest* request = new ZoneTransferRequest();
	request->requestID = ++currentRequestID;
	request->callback = callback;

	this->requests.push_back(request);

	MasterPackets::SendZoneTransferRequest(server, request->requestID, mythranShift, zoneID, zoneClone);
}

//! Handles a zone transfer response
void ZoneInstanceManager::HandleRequestZoneTransferResponse(uint64_t requestID, Packet* packet) {

	bool mythranShift = static_cast<bool>(packet->data[16]);
	uint32_t zoneID = PacketUtils::ReadPacketU32(17, packet);
	uint32_t zoneInstance = PacketUtils::ReadPacketU32(21, packet);
	uint32_t zoneClone = PacketUtils::ReadPacketU32(25, packet);
	uint16_t serverPort = PacketUtils::ReadPacketU16(29, packet);
	std::string serverIP = PacketUtils::ReadString(31, packet, false);

	for (uint32_t i = 0; i < this->requests.size(); ++i) {
		if (this->requests[i]->requestID == requestID) {

			// Call the request callback
			this->requests[i]->callback(mythranShift, zoneID, zoneInstance, zoneClone, serverIP, serverPort);

			delete this->requests[i];
			this->requests.erase(this->requests.begin() + i);
			return;
		}
	}

}

void ZoneInstanceManager::CreatePrivateZone(dServer* server, uint32_t zoneID, uint32_t zoneClone, const std::string& password) {
	MasterPackets::SendZoneCreatePrivate(server, zoneID, zoneClone, password);
}

void ZoneInstanceManager::RequestPrivateZone(
	dServer* server,
	bool mythranShift,
	const std::string& password,
	std::function<void(bool, uint32_t, uint32_t, uint32_t, std::string, uint16_t)> callback) {
	ZoneTransferRequest* request = new ZoneTransferRequest();
	request->requestID = ++currentRequestID;
	request->callback = callback;

	this->requests.push_back(request);

	MasterPackets::SendZoneRequestPrivate(server, request->requestID, mythranShift, password);
}
