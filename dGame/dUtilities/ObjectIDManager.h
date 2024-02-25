#pragma once

// C++
#include <functional>
#include <vector>
#include <stdint.h>

/*!
  \file ObjectIDManager.h
  \brief A manager for handling object ID generation
 */

//! The Object ID Manager
namespace ObjectIDManager {
	//! Requests a persistent ID
	/*!
	  \param callback The callback function
	 */
	void RequestPersistentID(const std::function<void(uint32_t)> callback);


	//! Handles a persistent ID response
	/*!
	  \param requestID The request ID
	  \param persistentID The persistent ID
	 */
	void HandleRequestPersistentIDResponse(const uint64_t requestID, const uint32_t persistentID);

	//! Generates an object ID server-sided
	/*!
	  \return A generated object ID
	 */
	uint32_t GenerateObjectID();

	//! Generates a random object ID server-sided
	/*!
	 \return A generated object ID
	 */
	uint32_t GenerateRandomObjectID();
};
