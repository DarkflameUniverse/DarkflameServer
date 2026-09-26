#pragma once

#include <functional>
#include <map>
#include <vector>
#include <string>

// RakNet
#include "RakNetTypes.h"
#include "Stamps.h"

class dServer;

/*!
  \file ZoneInstanceManager.hpp
  \brief A class for handling zone transfers and zone-related functions
 */

//! The zone manager
class ZoneInstanceManager {
private:
	static ZoneInstanceManager* m_Address;             //!< The singleton instance

	using TransferCallback = std::function<void(bool, uint32_t, uint32_t, uint32_t, std::string, uint16_t)>;
	// Also gets the login stamps that came back with the response (see Stamps.h)
	using StampedTransferCallback = std::function<void(bool, uint32_t, uint32_t, uint32_t, std::string, uint16_t, Stamps)>;
	std::map<uint64_t, StampedTransferCallback> requests; //!< The zone transfer requests
	uint64_t currentRequestID;              //!< The current request ID

public:

	//! The singleton method
	static ZoneInstanceManager* Instance() {
		if (m_Address == nullptr) {
			m_Address = new ZoneInstanceManager;
			m_Address->currentRequestID = 0;
		}

		return m_Address;
	}

	//! Requests a zone transfer
	/*!
	  \param zoneID The zone ID
	  \param zoneClone The zone clone
	  \param mythranShift Whether or not this is a mythran shift
	  \param callback The callback function
	 */
	void RequestZoneTransfer(dServer* server, uint32_t zoneID, uint32_t zoneClone, bool mythranShift, TransferCallback callback);

	//! Requests a zone transfer for a login: the stamps travel to master, which adds its own, and come back to the callback
	void RequestZoneTransfer(dServer* server, uint32_t zoneID, uint32_t zoneClone, bool mythranShift, const Stamps& stamps, StampedTransferCallback callback);

	//! Handles a zone transfer response
	/*!
	  \param requestID The request ID
	  \param packet The packet
	 */
	void HandleRequestZoneTransferResponse(Packet* packet);

	void CreatePrivateZone(dServer* server, uint32_t zoneID, uint32_t zoneClone, const std::string& password);

	void RequestPrivateZone(dServer* server, bool mythranShift, const std::string& password, TransferCallback callback);
};
