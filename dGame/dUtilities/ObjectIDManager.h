#pragma once

// C++
#include <functional>
#include <vector>
#include <stdint.h>

/**
 *  There are 2 types of IDs:
 *  Persistent IDs - These are used for anything that needs to be persist between worlds.
 *  Ephemeral IDs - These are used for any objects that only need to be unique for this world session.
 */

namespace ObjectIDManager {

	/**
	 * @brief Returns a Persistent ID with the CHARACTER bit set.
	 * 
	 * @return uint64_t A unique persistent ID with the CHARACTER bit set.
	 */
	uint64_t GetPersistentID();

	/**
	 * @brief Generates an ephemeral object ID for non-persistent objects.
	 * 
	 * @return uint32_t 
	 */
	uint32_t GenerateObjectID();
};
