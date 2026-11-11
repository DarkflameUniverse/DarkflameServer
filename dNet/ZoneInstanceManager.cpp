#include "ZoneInstanceManager.h"

// Custom Classes
#include "MasterPackets.h"

// Static Variables
ZoneInstanceManager* ZoneInstanceManager::m_Address = nullptr;

//! Requests a zone transfer
void ZoneInstanceManager::RequestZoneTransfer(dServer* server, uint32_t zoneID, uint32_t zoneClone, bool mythranShift, TransferCallback callback) {
	RequestZoneTransfer(server, zoneID, zoneClone, mythranShift, Stamps{}, [callback](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort, Stamps) {
		callback(mythranShift, zoneID, zoneInstance, zoneClone, serverIP, serverPort);
	});
}

void ZoneInstanceManager::RequestZoneTransfer(dServer* server, uint32_t zoneID, uint32_t zoneClone, bool mythranShift, const Stamps& stamps, StampedTransferCallback callback) {
	const auto nextID = ++currentRequestID;
	requests[nextID] = callback;

	MasterPackets::RequestZoneTransfer request;
	request.requestID = nextID;
	request.mythranShift = mythranShift;
	request.zoneID = zoneID;
	request.cloneID = zoneClone;
	request.stamps = stamps;
	MasterPackets::SendToMaster(request, server);
}

//! Handles a zone transfer response
void ZoneInstanceManager::HandleRequestZoneTransferResponse(const MasterPackets::RequestZoneTransferResponse& response) {
	const auto entry = requests.find(response.requestID);
	if (entry != requests.end()) {
		entry->second(response.mythranShift > 0, response.zoneID, response.zoneInstance, response.zoneClone, response.serverIP.string, response.serverPort, response.stamps);
		requests.erase(entry);
	}
}

void ZoneInstanceManager::CreatePrivateZone(dServer* server, uint32_t zoneID, uint32_t zoneClone, const std::string& password) {
	MasterPackets::CreatePrivateZone request;
	request.zoneID = zoneID;
	request.cloneID = zoneClone;
	request.password = password;
	MasterPackets::SendToMaster(request, server);
}

void ZoneInstanceManager::RequestPrivateZone(
	dServer* server,
	bool mythranShift,
	const std::string& password,
	TransferCallback callback) {
	const auto nextID = ++currentRequestID;
	requests[nextID] = [callback](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort, Stamps) {
		callback(mythranShift, zoneID, zoneInstance, zoneClone, serverIP, serverPort);
	};

	MasterPackets::RequestPrivateZone request;
	request.requestID = nextID;
	request.mythranShift = mythranShift;
	request.password = password;
	MasterPackets::SendToMaster(request, server);
}
