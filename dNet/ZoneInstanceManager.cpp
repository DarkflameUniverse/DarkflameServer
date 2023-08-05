#define _VARIADIC_MAX 10
#include "ZoneInstanceManager.h"

// Custom Classes
#include "MasterPackets.h"
#include "BitstreamUtils.h"
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
	CINSTREAM_SKIP_HEADER;
	uint64_t requestIDAgain;
	inStream.Read(requestIDAgain);
	bool mythranShift;
	uint8_t tmp;
	inStream.Read(tmp);
	mythranShift = tmp > 0;
	uint32_t zoneID;
	inStream.Read(zoneID);
	uint32_t zoneInstance;
	inStream.Read(zoneInstance);
	uint32_t zoneClone;
	inStream.Read(zoneClone);
	uint16_t serverPort;
	inStream.Read(serverPort);
	LUString serverIP(255);
	inStream.Read(serverIP);

	for (uint32_t i = 0; i < this->requests.size(); ++i) {
		if (this->requests[i]->requestID == requestID) {

			// Call the request callback
			this->requests[i]->callback(mythranShift, zoneID, zoneInstance, zoneClone, serverIP.string, serverPort);

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
