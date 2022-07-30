#pragma once

// C++
#include <cstdint>

class dLogger;

/*!
  \file ObjectIDManager.hpp
  \brief A manager that handles requests for object IDs
 */

 //! The Object ID Manager
class ObjectIDManager {
private:
	dLogger* mLogger;
	static ObjectIDManager* m_Address;         //!< The singleton instance

	uint32_t currentPersistentID;               //!< The highest current persistent ID in use

public:

	//! Return the singleton if it is initialized
	static ObjectIDManager* TryInstance() {
		return m_Address;
	}

	//! The singleton method
	static ObjectIDManager* Instance() {
		if (m_Address == nullptr) {
			m_Address = new ObjectIDManager;
		}

		return m_Address;
	}

	//! Initializes the manager
	void Initialize(dLogger* logger);

	//! Generates a new persistent ID
	/*!
	  \return The new persistent ID
	 */
	uint32_t GeneratePersistentID(void);

	void SaveToDatabase();
};
