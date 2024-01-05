#pragma once

// C++
#include <cstdint>

/*!
  \file PersistentIDManager.h
  \brief A manager that handles requests for object IDs
 */

 //! The Object ID Manager
namespace PersistentIDManager {
	//! Initializes the manager
	void Initialize();

	//! Generates a new persistent ID
	/*!
	  \return The new persistent ID
	 */
	uint32_t GeneratePersistentID();

	void SaveToDatabase();
};
