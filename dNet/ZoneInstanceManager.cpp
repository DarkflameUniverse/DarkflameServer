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

	MasterPackets::SendZoneTransferRequest(server, nextID, mythranShift, zoneID, zoneClone, stamps);
}

//! Handles a zone transfer response
void ZoneInstanceManager::HandleRequestZoneTransferResponse(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	uint64_t requestID;
	inStream.Read(requestID);
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
	Stamps stamps;
	if (!stamps.Deserialize(inStream)) stamps = {};

	const auto entry = requests.find(requestID);
	if (entry != requests.end()) {
		entry->second(mythranShift, zoneID, zoneInstance, zoneClone, serverIP.string, serverPort, stamps);
		requests.erase(entry);
	}
}

void ZoneInstanceManager::CreatePrivateZone(dServer* server, uint32_t zoneID, uint32_t zoneClone, const std::string& password) {
	MasterPackets::SendZoneCreatePrivate(server, zoneID, zoneClone, password);
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

	MasterPackets::SendZoneRequestPrivate(server, nextID, mythranShift, password);
}
