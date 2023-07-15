#pragma once

#include <functional>
#include <vector>
#include <string>

#include "RakNetTypes.h"

class dServer;
using ZoneTransferCallback = std::function<void(bool, uint32_t, uint32_t, uint32_t, std::string, uint16_t)>;

// The zone request
struct ZoneTransferRequest {
	uint64_t requestID = 0;
	ZoneTransferCallback callback;
};

//! The zone manager
class ZoneInstanceManager {
private:
	std::vector<ZoneTransferRequest> requests; //!< The zone transfer requests
	uint64_t currentRequestID;              //!< The current request ID
public:
	/**
	 * @param zoneID The zone ID
	 * @param zoneClone The zone clone
	 * @param mythranShift Whether or not this is a mythran shift
	 * @param callback The callback function
	 */
	void RequestZoneTransfer(dServer* server, uint32_t zoneID, uint32_t zoneClone, bool mythranShift, ZoneTransferCallback callback);

	/**
	 * @param requestID The request ID
	 * @param packet The packet
	 */
	void HandleRequestZoneTransferResponse(uint64_t requestID, Packet* packet);

	void CreatePrivateZone(dServer* server, uint32_t zoneID, uint32_t zoneClone, const std::string& password);

	void RequestPrivateZone(dServer* server, bool mythranShift, const std::string& password, ZoneTransferCallback callback);

};
