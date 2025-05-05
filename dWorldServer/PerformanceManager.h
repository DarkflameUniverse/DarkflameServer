#pragma once

#include <cstdint>
#include <map>

#include "dCommonVars.h"

using PerformanceProfile = uint32_t;

namespace PerformanceManager {
	/* Sets a performance profile for a given world. */
	void SelectProfile(LWOMAPID mapID);

	/* Gets the frame millisecond delta. Will return a higher value if the zone is empty. */
	uint32_t GetServerFrameDelta();
};
