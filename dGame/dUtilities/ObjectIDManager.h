#pragma once

// C++
#include <functional>
#include <vector>
#include <stdint.h>

#include "dCommonVars.h"

/**
 *  Also built into the DashboardServer, which adds items and models the same way.
 *
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

	struct ModelIDs {
		LWOOBJID modelID = LWOOBJID_EMPTY;
		LWOOBJID blueprintID = LWOOBJID_EMPTY; // the UGC model's ID
	};

	/**
	 * @brief Persistent IDs for a new player-built model, retrying past IDs already in use.
	 *
	 * @return ModelIDs The placed model's ID and its blueprint's ID.
	 */
	ModelIDs GetNewModelIDs();

	/**
	 * @brief Generates an ephemeral object ID for non-persistent objects.
	 * 
	 * @return uint32_t 
	 */
	uint32_t GenerateObjectID();
};
