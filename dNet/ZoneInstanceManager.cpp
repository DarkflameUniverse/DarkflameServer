#include "ZoneInstanceManager.h"

#include "MasterPackets.h"
#include "PacketUtils.h"
#include "dServer.h"

#include <algorithm>
#include <future>

//! Requests a zone transfer
void ZoneInstanceManager::RequestZoneTransfer(dServer* server, uint32_t zoneID, uint32_t zoneClone, bool mythranShift, ZoneTransferCallback callback) {

	ZoneTransferRequest request{};
	request.requestID = ++currentRequestID;
	request.callback = callback;

	this->requests.push_back(request);

	MasterPackets::SendZoneTransferRequest(server, request.requestID, mythranShift, zoneID, zoneClone);
}

//! Handles a zone transfer response
void ZoneInstanceManager::HandleRequestZoneTransferResponse(uint64_t requestID, Packet* packet) {

	bool mythranShift = static_cast<bool>(packet->data[16]);
	uint32_t zoneID = PacketUtils::ReadPacketU32(17, packet);
	uint32_t zoneInstance = PacketUtils::ReadPacketU32(21, packet);
	uint32_t zoneClone = PacketUtils::ReadPacketU32(25, packet);
	uint16_t serverPort = PacketUtils::ReadPacketU16(29, packet);
	std::string serverIP = PacketUtils::ReadString(31, packet, false);

	auto transferRequest = std::find(requests.begin(), requests.end(), [requestID](const ZoneTransferRequest& transfer) {
		return requestID == transfer.requestID;
		});
	if (transferRequest == requests.end()) return;

	// Now begin the callback since we confirmed we have found the matching request.
	transferRequest->callback(mythranShift, zoneID, zoneInstance, zoneClone, serverIP, serverPort);

	requests.erase(transferRequest);
}

void ZoneInstanceManager::CreatePrivateZone(dServer* server, uint32_t zoneID, uint32_t zoneClone, const std::string& password) {
	MasterPackets::SendZoneCreatePrivate(server, zoneID, zoneClone, password);
}

void ZoneInstanceManager::RequestPrivateZone(
	dServer* server,
	bool mythranShift,
	const std::string& password,
	ZoneTransferCallback callback) {
	ZoneTransferRequest request;
	request.requestID = ++currentRequestID;
	request.callback = callback;

	requests.push_back(request);

	MasterPackets::SendZoneRequestPrivate(server, request.requestID, mythranShift, password);
}
