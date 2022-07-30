#pragma once

// C++
#include <functional>
#include <vector>
#include <stdint.h>

/*!
  \file ObjectIDManager.hpp
  \brief A manager for handling object ID generation
 */

 //! The persistent ID request
struct PersistentIDRequest {
	uint64_t requestID;

	std::function<void(uint32_t)> callback;
};

//! The Object ID Manager
class ObjectIDManager {
private:
	static ObjectIDManager* m_Address;         //!< The singleton instance

	std::vector<PersistentIDRequest*> requests; //!< All outstanding persistent ID requests
	uint64_t currentRequestID;                  //!< The current request ID

	uint32_t currentObjectID;                   //!< The current object ID

public:

	//! The singleton instance
	static ObjectIDManager* Instance() {
		if (m_Address == 0) {
			m_Address = new ObjectIDManager;
		}

		return m_Address;
	}

	//! Initializes the manager
	void Initialize(void);

	//! Requests a persistent ID
	/*!
	  \param callback The callback function
	 */
	void RequestPersistentID(std::function<void(uint32_t)> callback);


	//! Handles a persistent ID response
	/*!
	  \param requestID The request ID
	  \param persistentID The persistent ID
	 */
	void HandleRequestPersistentIDResponse(uint64_t requestID, uint32_t persistentID);

	//! Generates an object ID server-sided
	/*!
	  \return A generated object ID
	 */
	uint32_t GenerateObjectID(void);

	//! Generates a random object ID server-sided
	/*!
	 \return A generated object ID
	 */
	static uint32_t GenerateRandomObjectID();

	//! Generates a persistent object ID server-sided
	/*!
	  \return A generated object ID
	 */
	uint32_t GeneratePersistentObjectID(void);
};
