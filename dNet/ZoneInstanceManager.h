#pragma once

// C++
#define _VARIADIC_MAX 10
#include <functional>
#include <vector>
#include <string>

// RakNet
#include <RakNetTypes.h>

class dServer;

/*!
  \file ZoneInstanceManager.hpp
  \brief A class for handling zone transfers and zone-related functions
 */

 //! The zone request
struct ZoneTransferRequest {
	uint64_t requestID;
	std::function<void(bool, uint32_t, uint32_t, uint32_t, std::string, uint16_t)> callback;
};

//! The zone manager
class ZoneInstanceManager {
private:
	static ZoneInstanceManager* m_Address;             //!< The singleton instance

	std::vector<ZoneTransferRequest*> requests; //!< The zone transfer requests
	uint64_t currentRequestID;              //!< The current request ID

public:

	//! The singleton method
	static ZoneInstanceManager* Instance() {
		if (m_Address == 0) {
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
	void RequestZoneTransfer(dServer* server, uint32_t zoneID, uint32_t zoneClone, bool mythranShift, std::function<void(bool, uint32_t, uint32_t, uint32_t, std::string, uint16_t)> callback);

	//! Handles a zone transfer response
	/*!
	  \param requestID The request ID
	  \param packet The packet
	 */
	void HandleRequestZoneTransferResponse(uint64_t requestID, Packet* packet);

	void CreatePrivateZone(dServer* server, uint32_t zoneID, uint32_t zoneClone, const std::string& password);

	void RequestPrivateZone(dServer* server, bool mythranShift, const std::string& password, std::function<void(bool, uint32_t, uint32_t, uint32_t, std::string, uint16_t)> callback);

};
